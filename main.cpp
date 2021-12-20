#include <iostream>
#include "decoder.h"
#include <array>
#include "definitioner.h"
#include "processor.h"
#include "sampler.h"
#include <SFML/Audio.hpp>
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <SFML/System/Time.hpp>
#include <SFML/System.hpp>
#include <vector>
#include <functional>
#include <atomic>
#include <windows.h>
#include <string>
#include <vector>
#include <array>
#include <valarray>
#include <ccomplex>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <vector>
#include <queue>
#include <array>
#include <cstdlib>
#include <functional>

using namespace std;


int main() {
        decoder::run(true);
        decoder::threadInstant();
        decoder::end();
    return 0;
}