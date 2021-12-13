#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <queue>
#include <array>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <windows.h>
#include <iostream>
#include "decoder.h"
#include "sampler.h"
//#include "sampler2.h"
#include <stdlib.h>
#include "processor.h"
#include "toolbox.h"
#include <algorithm>
//Private deklarationer

namespace decoder
{
    using namespace std::chrono;


    std::atomic<state>					status = state::uninitialized;
    std::deque<std::vector<short>>		queue;
    std::vector<short>					buffer;
    std::mutex							queueMutex;
    std::condition_variable				queueCondition;
    std::thread							worker;
    sampler*							rec1;
    //std::vector<short>                  endelig;
    std::vector<short>                  fft;
    int count=0;
    bool								allowPlayback = false;
    std::atomic<bool>					running;

    std::array<float, 8>				previousGoertzelArray;
    bool								previousThresholdBroken;
    int									previousToneId;

    high_resolution_clock				clock;
    time_point<high_resolution_clock>	debounce;

    std::function<void(uint)>			callback;

    // Private metoder
    void								threadWindowed();

    std::vector<float>					decode(std::vector<short> &samples);
    void								decode2(std::vector<short> &samples);

    void								appendQueue(std::vector<short> samples);

    bool								thresholdTest(std::array<float, 8> goertzelArray);
    std::array<int, 2>					extractIndexes(std::array<float, 8> &goertzelArray);
    int									extractToneId(std::array<int, 2> &indexes);
    void								yndlings();
}

//Lav og kør ny sampler, og start ny thread
void decoder::run(bool allowPlayback)
{
    // initialize variables
    decoder::callback		= callback;
    decoder::rec1			= new sampler(&decoder::appendQueue, allowPlayback);
    decoder::allowPlayback	= allowPlayback;

    // start sampler
    decoder::rec1->start(SAMPLE_RATE);
    decoder::status			= state::running;
    std::cout << "stratsample" << std::endl;

    // start worker thread
    decoder::previousToneId = -1;
    decoder::debounce		= decoder::clock.now();
    decoder::running		= true;
    decoder::worker			= std::thread(&decoder::threadInstant);
    //decoder::worker			= std::thread(&decoder::yndlings);
    std::cout << "lavet worker" << std::endl;

}


// End the decoder
void decoder::end()
{
    // stop and delete sampler
    decoder::rec1->stop();
    delete decoder::rec1;

    // stop worker thread
    decoder::status		= state::uninitialized;
    decoder::running	= false;
    decoder::worker.join();
}


// Thread function (step window decoding / chunk combination)
void decoder::threadWindowed()
{
    while (decoder::running)
    {
        // check status
        if (decoder::status != state::running)
        {
            continue;
        }

        // queue critical section
        decoder::queueMutex.lock();

        // check that queue has enough elements
        if (decoder::queue.size() < STEP_WINDOW_SIZE)
        {
            decoder::queueMutex.unlock();
            continue;
        }

        // clear buffer
        decoder::buffer.clear();

        // compose buffer from queue
        for (const auto& sampleChunks : decoder::queue)
        {
            decoder::buffer.insert(decoder::buffer.end(), sampleChunks.begin(), sampleChunks.end());
        }

        // pop first element of queue
        decoder::queue.pop_front();

        decoder::queueMutex.unlock();

        // decode the copied samples
        decoder::decode(buffer);
    }
}

// Thread function (instant decoding)
void decoder::threadInstant()
{
    while (decoder::running)
    {
        /*
        The method is protected by a mutex, which is locked using scope guard std::unique_lock. If the queue is empty,
        we wait on the condition variable queueCondition. This releases the lock to other threads and blocks until we are
        notified that the condition has been met.
        https://juanchopanzacpp.wordpress.com/2013/02/26/concurrent-queue-c11/
        */

        // acquire queue mutex
        std::unique_lock<std::mutex> queueLock(decoder::queueMutex);

        // check if queue empty; wait for conditional variable if false
        while (queue.empty())
        {
            decoder::queueCondition.wait(queueLock);
            //std::cout << "tom" << std::endl;
        }

        // make copy of first element in queue
        auto samplesCopy = decoder::queue.front();

        // pop the element
        decoder::queue.pop_front();

        // unclock queue
        queueLock.unlock();

        // decode the copied samples
        decoder::decode(samplesCopy);

    }
}

// Add a (copy of) vector of samples to decoding queue
void decoder::appendQueue(std::vector<short> samples)
{
    //std::cout << samples.size();
    // critical section
    decoder::queueMutex.lock();

    //std::cout << "[QUEUE] Adding to queue with [" << decoder::queue.size() << "] elements.\n";

    // segregate chunk if it contains multiple (i.e. size > ~441)
    if (samples.size() > CHUNK_SIZE_MAX)
    {

        // variables
        std::vector<std::vector<short>>		chunks{};

        auto itr					= samples.cbegin();
        int const numberOfChunks	= samples.size() / CHUNK_SIZE;
        int const remainder			= samples.size() % CHUNK_SIZE;

        // create vector of chunks
        for (int i = 0; i < numberOfChunks; i++)
        {
            chunks.emplace_back(std::vector<short>{itr, itr + CHUNK_SIZE});
            itr += CHUNK_SIZE;
        }

        // append remaining samples to last chunk
        if (remainder > 0)
        {
            chunks[numberOfChunks - 1].insert(chunks[numberOfChunks - 1].end(), samples.end() - remainder, samples.end());
        }

        // append chunks onto queue
        for (const auto& chunk : chunks)
        {
            decoder::queue.push_back(chunk);
        }
    }
    else
    {
        // push chunk onto queue
        decoder::queue.push_back(samples);
    }

    decoder::queueMutex.unlock();

    decoder::queueCondition.notify_one();
}

// Test whether two of magnitudes surpass their according thresholds; return boolean
bool decoder::thresholdTest(std::array<float, 8> goertzelArray)
{
    int counter = 0;

    for (int i = 0; i < 8; i++)
    {
        if (goertzelArray[i] > freqThresholds[i])
        {
            counter++;
        }
    }

    return (counter == 2);
}

// Convert goertzelArray to indexes (row & column) of two most prominent frequencies; return array[2]
std::array<int, 2> decoder::extractIndexes(std::array<float, 8> &goertzelArray)
{
    float magnitudeLowMax		= 0;
    float magnitudeHighMax		= 0;

    std::array<int, 2> indexes	= { -1, -1 };

    // Iterate through array of goertzel magnitudes
    for (uint i = 0; i < goertzelArray.size(); i++)
    {
        // define magnitude & threshold for current frequency index (i)
        auto magnitude = goertzelArray[i] * freqMultiplier[i];
        auto threshold = freqThresholds[i] * ((previousToneId == -1) ? TH_MULTIPLIER_H : TH_MULTIPLIER_L);

        // low frequencies
        if (i < 4 && magnitude > threshold && magnitude > magnitudeLowMax)
        {
            magnitudeLowMax = magnitude;
            indexes[0] = i;
        }

            // high frequencies
        else if (i >= 4 && magnitude > threshold && magnitude > magnitudeHighMax)
        {
            magnitudeHighMax = magnitude;
            indexes[1] = (i - 4);
        }
    }

    // return array of indexes
    return indexes;
}

// Convert DTMF indexes (row & column) to toneID (0-15); return int
int decoder::extractToneId(std::array<int, 2> &indexes)
{
    int indexLow	= indexes[0];
    int indexHigh	= indexes[1];

    if (indexHigh < 0 || indexLow < 0)
    {
        return -1;
    }

    return (indexLow * 4 + indexHigh);
}

// Decode a chunk of samples from the queue
std::vector<float> decoder::decode(std::vector<short> &samples) {
    /*
    for(int i=0; i<samples.size();i++){
        endelig.push_back(samples[i]);
    }
     */
    // check debounce
    if (static_cast<duration<double, std::milli>>(decoder::clock.now() - decoder::debounce).count() < DEBOUNCE) {
        //
    }


    // log
    //std::cout << "[DECODER] Decoding [" << samples.size() << "] samples...\n\n";

    // update status
    decoder::status = state::working;
    //std::cout << "status working" << std::endl;
    // apply hanning window
    //processor::hannWindow(samples);
    //Apply fft?
    //cArray f;
    //f=processor::fft(samples);
    /*
    for (int i=0; i<f.size();i++){
        std::cout << f[i] << std::endl;
    }
     */
    //for(int i=0; i<f.size();i++) {
    //std::cout << "fft'et: " << f[i] << std::endl;
    //}
    // apply zero padding if chunk too small
    //std::cout << "sample size" << samples.size() << std::endl;
    if (samples.size() < CHUNK_SIZE_MIN) {
        processor::zeroPadding(samples, CHUNK_SIZE_MIN);
        //std::cout << "samples.size() > ChunkMAX" << std::endl;
    }

    // compile goertzelArray for all DTMF frequencies
    //auto goertzelArray = processor::goertzelArray(samples);

    // extract indexes (row & column) of most prominent frequencies
    //auto indexes = decoder::extractIndexes(goertzelArray);

    // check harmonies
    ;

    // convert indexes to DTMF toneId
    //auto toneId = decoder::extractToneId(indexes);

    // redundancy check & callback
    /*
    if (toneId >= 0 && decoder::previousToneId == toneId)
    {
        decoder::debounce = decoder::clock.now();
        decoder::previousToneId = -1;
        //processor::printGoertzelArray(goertzelArray);
        decoder::callback(toneId);
    }
    else if (previousToneId == -1)
    {
        decoder::previousToneId = toneId;
    }
    else
    {
        decoder::previousToneId = -1;
    }
    */
    // update status
    decoder::status = state::running;
    //count++;

    //if(endelig.size()>=43000){
    //dtmf::toolbox::exportAudio(endelig, "teeeeest.ogg");
    //std::ofstream myfile;
    //myfile.open("stream.txt");
    //for(int j=0; j<endelig.size();j++){
    //    myfile << endelig[j] << " ";
    //}
    cArray f;
    f = processor::fft(samples);
    f = f.cshift(f.size() / 2);
    //TODO: Find peak af hele signalet, og lav et nyt array fra peak til slut(eller start) alt efter om peak er over eller under f.size()/2 og bagefter find peak af den nye
    //TODO: Del array op i lav og høj index: 697=227.97,   852=229.5         1209=233.09    1477=235.77
    //for(int i=227;i<231;i++
    std::ofstream myfile;
    myfile.open("FFTstream.txt");

    for (int i = 0; i < f.size() / 2; i++) {
        f[i] = 0;
    }
    //Fra peak index minus X (shoulder) skal der laves et nyt index fra
    //Find peak af det
    //TODO Fix frekvensfejl (indexering) og find DTMF fejlmargin


    for (int i = 0; i < f.size(); i++) {
        myfile << abs(f[i]) << " ";
    }
    int HiLowest = 1080, HiHighest = 1093;
    int indexHi = HiLowest;
    float largest_elementHi = abs(f[HiLowest]);
    for (int j = HiLowest; j < HiHighest - 1; j++) {
        if (largest_elementHi < abs(f[j + 1])) {
            largest_elementHi = abs(f[j + 1]);
            indexHi = j + 1;
        }
    }
    float frekvensHi = (indexHi - samples.size() / 2) * (SAMPLE_RATE / samples.size()); //Pga cshift/fftshift
    int LoLowest = 1056, LoHighest = 1063;
    int indexLo = LoLowest;
    float largest_elementLo = abs(f[LoLowest]);
    for (int j = LoLowest; j < LoHighest - 1; j++) {
        if (largest_elementLo < abs(f[j + 1])) {
            largest_elementLo = abs(f[j + 1]);
            indexLo = j + 1;
        }
    }
    float frekvensLo = (indexLo - samples.size() / 2) * (SAMPLE_RATE / samples.size());
    //std::cout << "Lo index: " << indexLo << std::endl;
    //std::cout << "Hi index: " << indexHi << std::endl;
    //std::cout << "Største: " <<  largest_element << std::endl;
    //std::cout << "f.size(): " << f.size() << std::endl;
    //TODO: Del cArray op i  i frekvenser fra 1209-1633 (+-50) og i 697-941(+-50)
    std::vector<float> res{0,0};
    if (abs(f[indexHi]) > 100000 && abs(f[indexLo]) > 30000) {
    std::cout << "Low frekvensen er: " << frekvensLo * 1.035 << std::endl;
    std::cout << "High frekvensen er: " << frekvensHi * 1.0295 << std::endl;
    res[0]=frekvensLo;
    res[1]=frekvensHi;
}
    //for(int i=0; i<f.size();i++)
    //{
    //    fft.push_back(abs(f[i]));
    //}
    // int largest_element;
    // for(int i=0; i<fft.size();i++){
    //     if(abs(fft[i])<abs(fft[i+1])){
    //        largest_element=abs(fft[i+1]);
    //     }
    // }

    //Index(peak)-Fs/N/2
    //Fs/N=frequency resolution

    //dtmf::toolbox::exportAudio(endelig, "teeeeest.ogg");
    //decoder::end();
    //abort();
    //decoder::extractFrequency(res);
    //TODO: Ret extractFreq til float
    return res;
}
//}
// Decode a chunk of samples from the queue (with threshold breaking)
void decoder::decode2(std::vector<short> &samples)
{
    decoder::status = state::working;

    // decode
    //std::cout << "[DECODER] Decoding [" << samples.size() << "] samples...\n\n";

    // check debounce
    if ((int)static_cast<duration<double, std::milli>>(decoder::clock.now() - decoder::debounce).count() < OLD_DEBOUNCE)
    {
        decoder::status = state::running;
        std::cout << "kører" << std::endl;
        return;
    }

    // apply hanning window to samples
    processor::hannWindow(samples);

    // compile goertzelArray for all DTMF frequencies
    auto goertzelArray = processor::goertzelArray(samples);
    std::cout << "goertzel" << std::endl;
    // check if any values surpass thresholds
    bool thresholdBroken = decoder::thresholdTest(goertzelArray);

    /*
    A surpassed boolean is needed for every DTMF frequency!	Also update
    thresholdTest so only DTMF tones pass through (>4).
    */

    // check if previous treshold was surpassed and current was not
    if (previousThresholdBroken && !thresholdBroken)
    {
        // extract indexes (row & column) of most prominent frequencies of previousGoertzel
        auto indexes = decoder::extractIndexes(previousGoertzelArray);

        // convert indexes to DTMF toneID
        auto toneId = decoder::extractToneId(indexes);
        std::cout << "kører" << std::endl;
        // callback
        if (toneId >= 0)
        {
            decoder::debounce = decoder::clock.now();
            decoder::callback(toneId);
        }
    }

    // save values for next iteration
    previousGoertzelArray		= goertzelArray;
    previousThresholdBroken		= thresholdBroken;

    decoder::status = state::running;
    std::cout << "kører" << std::endl;
}

int decoder::extractFrequency(std::vector<int> res) {
    int lo=-1;
    int hi=-1;
    for(int i=0; i<3;i++){
            if(res[0]==freqLo[i]){ //freqLo & Hi står i definitioner
                lo=i;
            }
            if(res[1]==freqHi[i]){
                hi=i;
            }
        }
    switch(lo){
        case 0:
            std::cout << "0" << std::endl;
            if(hi==0){
                std::cout << "1" << std::endl;
                return 1;
            }
            if(hi==1){
                std::cout << "2" << std::endl;
                return 2;
            }
            if(hi==2){
                std::cout << "3" << std::endl;
                return 3;
            }
            break;
        case 1:
            std::cout << "1" << std::endl;
            if(hi==0){
                std::cout << "4" << std::endl;
                return 4;
            }
            if(hi==1){
                std::cout << "5" << std::endl;
                return 5;
            }
            if(hi==2){
                std::cout << "6" << std::endl;
                return 6;
            }
            break;
        case 2:
            std::cout << "7" << std::endl;
            if(hi==0){
                std::cout << "7" << std::endl;
                return 1;
            }
            if(hi==1){
                std::cout << "8" << std::endl;
                return 8;
            }
            if(hi==2){
                std::cout << "9" << std::endl;
                return 9;
            }
            break;
    }


}