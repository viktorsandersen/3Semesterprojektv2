#include <thread>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <array>
#include <valarray>
#include <vector>
#include <deque>
#include <map>
#include <chrono>
#include <functional>
#include <atomic>
#include <windows.h>
#include <SFML/Audio.hpp>

#include "toolbox.h"

//#include "signal/generator.h"
#include "sampler.h"
#include "processor.h"
#include "decoder.h"


namespace dtmf {


    namespace toolbox {
        std::vector<short> convertSFBuffer(sf::SoundBuffer &buffer);
    }

// Export a vector of shorts as a data file
    void toolbox::exportSamples(std::vector<short> &samples, std::string filename) {

        std::ofstream outputStream(filename);

        for (const auto &sample: samples) {
            outputStream << sample << "\n";
        }

        outputStream.close();

        std::cout << "Samples array[" << samples.size() << "] exported as \"" << filename << "\" ...\n";
    }

    void toolbox::testSampler()
    {
        // variables
        int			counter = 0;
        int const	period = 100;
        std::map<int, int> testData;

        // setup sampler w/ callback
        sampler*	test = new sampler([&](std::vector<short> samples) {
            testData[counter++] = samples.size();
            //std::cout << "Got chunk [" << ++counter << "][" << samples.size() << "]\n";
        });

        // start sampler for 100ms
        test->start(SAMPLE_RATE);
        std::this_thread::sleep_for(std::chrono::milliseconds(period));
        test->stop();
        // log
        std::cout << "\nFinished test. Got " << counter << " chunks of expected " << period / SAMPLE_INTERVAL << ".\n";
        std::cout << "Data:\n";

        for (const auto& pair : testData)
        {
            std::cout << "[" << pair.first << "][" << pair.second << "]\n";
        }

        // clean up
        delete test;
    }


// Export a vector of shorts som en audio file
    void toolbox::exportAudio(std::vector<short> &samples, std::string filename) {
        // create data directory & update filename
        //toolbox::makeDataDirectory();
        //filename = DATA_PATH + filename;

        sf::SoundBuffer buffer;
        buffer.loadFromSamples(&samples[0], samples.size(), 1, SAMPLE_RATE);

        buffer.saveToFile(filename);
    }

// Plot vector of shorts using MATLAB - Does not quite work
    void toolbox::plotSamples(std::vector<short> &samples, std::string filename, std::array<std::string, 3> labels)
    {
        // export samples
        toolbox::exportSamples(samples, filename);

        // export plot function
        // or somehow include/copy the MATLAB scripts to the destination path
        // currently simply manual copy scripts folder to current working path of console-app

        std::cout << "Launching MatLab script ...\n";

        // run MATLAB script/function
        // needs to be changed to cd "/script"
        std::string cmd = "matlab -nodesktop -r \"plot_samples('" + std::string(DATA_PATH) + filename + "', '" + labels[0] + "', '" + labels[1] + "', '" + labels[2] + "')\"";
        system(cmd.c_str());
    }

// Convert an audio file to a samples array; return vector of shorts
    std::vector<short> toolbox::convertAudio(std::string filename) {
        std::cout << "Converting \"" << filename << "\" to array.\n";

        sf::SoundBuffer buffer;

        if (!buffer.loadFromFile(filename)) {
            return {0};
        }

        const short *data = &buffer.getSamples()[0];
        const int size = buffer.getSampleCount();

        std::vector<short> samples(data, data + size);

        return samples;
    }

// Convert SFML SoundBuffer to Vector
    std::vector<short> toolbox::convertSFBuffer(sf::SoundBuffer &buffer) {
        const short *data = &buffer.getSamples()[0];
        const int size = buffer.getSampleCount();

        std::vector<short> samples(data, data + size);
        return samples;
    }
};