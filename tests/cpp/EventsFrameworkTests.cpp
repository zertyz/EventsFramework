#include <iostream>
#include <functional>
using namespace std;

#include <boost/test/unit_test.hpp>
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE EventsFrameworkTests
#include <boost/test/unit_test.hpp>

#include <MutuaTestMacros.h>
#include <BetterExceptions.h>
#include <TimeMeasurements.h>
using namespace mutua::cpputils;


struct IndirectCallSuiteObjects {

    // test case constants
    static constexpr unsigned int numberOfCalls  = 4'096'000*512;
    static constexpr unsigned int numberOfPasses = 4;
    static constexpr unsigned int threads        = 4;

    // test case data
    static   std::vector  <std::string>* stdStringKeys;		// keys for all by EASTL containers

    // output messages for boost tests
    static string testOutput;


    IndirectCallSuiteObjects() {
    	static bool firstRun = true;
    	if (firstRun) {
			cerr << endl << endl;
			cerr << "Indirect Call Spikes:" << endl;
			cerr << "==================== " << endl << endl;
			firstRun = false;
    	}
    }
    ~IndirectCallSuiteObjects() {
    	BOOST_TEST_MESSAGE("\n" + testOutput);
    	testOutput = "";
    }

    static void output(string msg, bool toCerr) {
    	if (toCerr) cerr << msg << flush;
    	testOutput.append(msg);
    }
    static void output(string msg) {
    	output(msg, true);
    }

};
// static initializers
string IndirectCallSuiteObjects::testOutput = "";


static int _r = 1234;
void indirectlyCallableMethod(int p1) {
	_r ^= p1;
}

BOOST_FIXTURE_TEST_SUITE(IndirectCallSuite, IndirectCallSuiteObjects);

BOOST_AUTO_TEST_CASE(directCallSpikes) {
	HEAP_MARK();
	size_t r=19258499592934ul;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			indirectlyCallableMethod(r);
			r |= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("directCallSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("directCallSpikes", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(stdInvokeSpikes) {
	HEAP_MARK();
	size_t r=19258499592934ul;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			std::invoke(indirectlyCallableMethod, r);
			r |= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("stdInvokeSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("stdInvokeSpikes", output);
	output("r = " + to_string(r) + "\n");
}

template<typename _CallableProcedure> struct CallableProcedure {
	_CallableProcedure&& procedureReference;
	CallableProcedure(_CallableProcedure&& procedureReference) : procedureReference(procedureReference) {}
	template<typename _Argument> inline void invoke(_Argument&& argument) {std::invoke(procedureReference, argument);}
};
BOOST_AUTO_TEST_CASE(procedureTemplateSpikes) {
	HEAP_MARK();
	size_t r=19258499592934ul;
	CallableProcedure procedure(indirectlyCallableMethod);
	class param;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			procedure.invoke(r);
			r |= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("procedureTemplateSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("procedureTemplateSpikes", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(stdFunctionSpikes) {
	HEAP_MARK();
	std::function<void(int)> callableObject = indirectlyCallableMethod;
	size_t r=19258499592934ul;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			callableObject(r);
			r |= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("stdFunctionSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("stdFunctionSpikes", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_SUITE_END();


boost::unit_test::test_suite* init_unit_test_suite(int argc, char* args[]) {
	boost::unit_test::framework::master_test_suite().p_name.value = "EventsFrameworkTests";
	return 0;
}
