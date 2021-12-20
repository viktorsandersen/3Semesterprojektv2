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
#include <iostream>
#include "decoder.h"
#include "sampler.h"
#include <stdlib.h>
#include "processor.h"
#include "toolbox.h"
#include <algorithm>
#include "error.h"
//Private deklarationer

namespace decoder
{
    using namespace std::chrono;

    int counter=0;
    std::atomic<state>					status = state::uninitialized;
    std::deque<std::vector<short>>		queue;
    std::vector<short>					buffer;
    std::mutex							queueMutex;
    std::condition_variable				queueCondition;
    std::thread							worker;
    sampler*							rec1;
    cArray                              fft;
    char lastChar;
    bool								allowPlayback = false;
    std::atomic<bool>					running;
    std::vector<char>                   protocolBuffer;
    std::array<float, 8>				previousGoertzelArray;
    bool								previousThresholdBroken;
    int									previousToneId;

    high_resolution_clock				clock;
    time_point<high_resolution_clock>	debounce;

    std::function<void(uint)>			callback;

    // Private metoder
    void								threadWindowed();

    void            					decode(std::vector<short> &samples);
    void								decode2(std::vector<short> &samples);

    void								appendQueue(std::vector<short> samples);

    bool								thresholdTest(std::array<float, 8> goertzelArray);
    std::array<int, 2>					extractIndexes(std::array<float, 8> &goertzelArray);
    int									extractToneId(std::array<int, 2> &indexes);
    void                                bitSequence(char sentChar);
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

    // start worker thread
    decoder::previousToneId = -1;
    decoder::debounce		= decoder::clock.now();
    decoder::running		= true;
    decoder::worker			= std::thread(&decoder::threadInstant);
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
    // critical section
    decoder::queueMutex.lock();

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
        // Define magnitude & threshold for current frequency index (i)
        auto magnitude = goertzelArray[i] * freqMultiplier[i];
        auto threshold = freqThresholds[i] * ((previousToneId == -1) ? TH_MULTIPLIER_H : TH_MULTIPLIER_L);

        // Low frequencies
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


void decoder::extractFrequency(std::vector<int> res) {
    int hi=-1;
    int lo=-1;
    //Checks if the the low and high frequencies are a part of the DTMF table.
    for(int i=0; i<3; i++){
        if(res[1]==freqHi[i]){
            hi=i;
        }
        if(res[0]==freqLo[i]) {
            lo = i;
        }
    }
    switch(lo){
        case 0:
            if(hi==0){
                //std::cout << "1" << std::endl;
                decoder::bitSequence('1');
            }
            if(hi==1){
                //std::cout << "2" << std::endl;
                decoder::bitSequence('2');
            }
            if(hi==2){
                //std::cout << "3" << std::endl;
                decoder::bitSequence('3');
            }
            break;
        case 1:
            if(hi==0){
                //std::cout << "4" << std::endl;
                decoder::bitSequence('4');
            }
            if(hi==1){
                //std::cout << "5" << std::endl;
                decoder::bitSequence('5');
            }
            if(hi==2){
                //std::cout << "6" << std::endl;
                decoder::bitSequence('6');
            }
            break;
        case 2:
            if(hi==0){
                //std::cout << "7" << std::endl;
                decoder::bitSequence('7');
            }
            if(hi==1){
                //std::cout << "8" << std::endl;
                decoder::bitSequence('8');
            }
            if(hi==2){
                //std::cout << "9" << std::endl;
                decoder::bitSequence('9');
            }
            break;

    }


}
// Decode a chunk of samples from the queue
void decoder::decode(std::vector<short> &samples) {

    // check debounce
    if (static_cast<duration<double, std::milli>>(decoder::clock.now() - decoder::debounce).count() < DEBOUNCE) {
        //
    }

    // update status
    decoder::status = state::working;

    // apply hanning window
    //processor::hannWindow(samples);

    // apply zero padding if chunk too small
    if (samples.size() < CHUNK_SIZE_MIN) {
        processor::zeroPadding(samples, CHUNK_SIZE_MIN);
    }

    // update status
    decoder::status = state::running;

    fft = processor::fft(samples);
    fft = fft.cshift(fft.size() / 2);

    //Removes all the complex conjugate frequencies
    for (int i = 0; i < fft.size() / 2; i++) {
        fft[i] = 0;
    }

    //Saves a txt file for matlab with the FFT values
    std::ofstream myfile;
    myfile.open("FFTstream.txt");
    for (int i = 0; i < fft.size(); i++) {
        myfile << abs(fft[i]) << " ";
    }
    //The indexes the low frequencies has to be between to be DTMF ((44100/2048*index)-44100/2=1209) or ((index-2048/2)*44100/2048=1209)
    //-44100/2 is cshift
    int HiLowest = 1080, HiHighest = 1093;
    int indexHi = HiLowest;
    //Find the first peak
    float largest_elementHi = abs(fft[HiLowest]);
    for (int j = HiLowest; j < HiHighest - 1; j++) {
        if (largest_elementHi < abs(fft[j + 1])) {
            largest_elementHi = abs(fft[j + 1]);
            indexHi = j + 1;
        }
    }
    float frekvensHi = (indexHi - samples.size() / 2) * (SAMPLE_RATE / samples.size()); //Pga cshift/fftshift
    int LoLowest = 1056, LoHighest = 1063;
    int indexLo = LoLowest;
    float largest_elementLo = abs(fft[LoLowest]);
    for (int j = LoLowest; j < LoHighest - 1; j++) {
        if (largest_elementLo < abs(fft[j + 1])) {
            largest_elementLo = abs(fft[j + 1]);
            indexLo = j + 1;
        }
    }
    float frekvensLo = (indexLo - samples.size() / 2) * (SAMPLE_RATE / samples.size());
    std::vector<int> res{0,0};
    //If the two frequencies pass a certain threshold they are accepted
    if (abs(fft[indexHi]) > 100000 && abs(fft[indexLo]) > 30000) {
        res[0]=frekvensLo*1.035;
        res[1]=frekvensHi*1.0295;
        decoder::extractFrequency(res);
    }
    else {
        //Otherwise it sends an n to the protocol, showing a break in the sound
        decoder::bitSequence('n');
    }
}
// Decode a chunk of samples from the queue (with threshold breaking)
void decoder::decode2(std::vector<short> &samples)
{
    decoder::status = state::working;

    // decode
    //std::cout << "[DECODER] Decoding [" << samples.size() << "] samples...\n\n";
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

void decoder::bitSequence(char sentChar){
    //Timer should have been used, but this resembles about 1.2 seconds
    if(counter==30){
        protocolBuffer.clear();
        //Then the buffer is cleared, in order not to use and save old values
        std::cout << "clearet" << std::endl;
        counter=0;
    }
    //Don't want the buffer to be filled with the same frequency from one sound, so it only accepts 1 at a time
    if(lastChar!=sentChar){
        protocolBuffer.push_back(sentChar);
        counter=0;
        auto start = std::chrono::steady_clock::now();
    }
    counter++;
    lastChar=sentChar;
    if(protocolBuffer.size()==8){
        protocolBuffer.erase(std::remove(protocolBuffer.begin(), protocolBuffer.end(), 'n'), protocolBuffer.end());
        /* To show the final values
        std::cout << protocolBuffer[0] << std::endl;
        std::cout << protocolBuffer[1] << std::endl;
        std::cout << protocolBuffer[2] << std::endl;
        std::cout << protocolBuffer[3] << std::endl;
        std::cout << "stop" << std::endl;
         */
        signal_to_int(protocolBuffer);
        protocolBuffer.clear();
        counter=0;
    }
}