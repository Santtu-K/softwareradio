#pragma once

#include <uhd.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/property_tree.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/static.hpp>
#include <uhd/version.hpp>

/*
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/utils/static.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <uhd/version.hpp>
#include <uhd/property_tree.hpp>
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/msg.hpp>
*/


#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <fstream>
#include <ostream>
#include <csignal>
#include <stdlib.h>
#include <iostream>

#include <cstdlib>
#include <math.h>

#include <cmath>
#include <complex>
#include <vector>
#include <stdexcept>
#include <fftw3.h>

#include <stdint.h>
#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <iostream>
#include <list>
#include <map>
#include <string>

#include <thread>
#include <functional>
#include <chrono>
#include <mutex>
#include <atomic>

#include <signal.h>
#include <stdlib.h>

using namespace std;

using cFloat = std::complex<float>;

enum class SYNC_STATE{CELL_SEARCH,CELL_ID_SEARCH,CELL_TRACKING};
enum class CELL_SEARCH_STATE_PSS{PSS_SEARCH,PSS_FOUND,PSS_NOT_FOUND};
enum class CELLID_SEARCH_STATE_SSS{SSS_SEARCH,SSS_FOUND,SSS_NOT_FOUND};

