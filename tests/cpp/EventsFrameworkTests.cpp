#include <iostream>
#include <array>
#include <functional>
#include <cstring>
#include <mutex>
#include <future>
using namespace std;

#include <boost/test/unit_test.hpp>
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE EventsFrameworkTests
#include <boost/test/unit_test.hpp>

#include <MutuaTestMacros.h>
#include <BetterExceptions.h>
#include <TimeMeasurements.h>
using namespace mutua::cpputils;


template<typename _Argument>
struct EventDefinition {
	void(&&procedureReference)(_Argument);
	EventDefinition(void(&&procedureReference)(_Argument))
		: procedureReference(procedureReference) {}
};

enum class MyEvents {EventA, EventB, EventC, last=EventC};
template <typename _Argument, class E, class = std::enable_if_t<std::is_enum<E>{}>>
class EventFramework {
public:
	EventFramework(const EventDefinition<_Argument> (&eventHandlers)[((int)(E::last)) + 1]) {
		int eventsEnumerationLength = ((int)(E::last)) + 1;
		cerr << "Now at EventFramework!" << endl;
		cerr << "length of E:   " << eventsEnumerationLength << endl;
		cerr << "events.size(): " << sizeof(eventHandlers) / sizeof(eventHandlers[0]) << endl;
//		for (int i=0; i<events.size(); i++) {
//			int v = (int)static_cast<E>(static_cast<std::underlying_type_t<E>>(events[i]));
//			cerr << "events[" << i << "] = '" << v << "'" << endl;
//		}
	}
};
//E &operator ++ (E &e) {
//    return e = static_cast<E>(static_cast<std::underlying_type_t<E>>(e) + 1);
//}

template <typename _AnswerType, typename _ArgumentType, int _NListeners, int _NAnswers>
struct EventLink {

	// debug info
	string eventName;

	// consumer
	void (*answerlessConsumerProcedureReference) (const _ArgumentType&);
	void (*answerfullConsumerProcedureReference) (const _ArgumentType&, _AnswerType*, std::mutex&);
	_AnswerType* answerReferences[_NAnswers];
	std::mutex   answerMutexes[_NAnswers];

	// listeners
	void (*listenerProcedureReferences[_NListeners]) (const _ArgumentType&);
	int nListenerProcedureReferences;

	EventLink(string eventListenerName)
			: eventName(eventListenerName)
			, answerlessConsumerProcedureReference(nullptr)
			, answerfullConsumerProcedureReference(nullptr) {
		// initialize listeners
		memset(listenerProcedureReferences, '\0', sizeof(listenerProcedureReferences));
		nListenerProcedureReferences = 0;
		// initialize answers & mutexes
		memset(answerReferences,    '\0', sizeof(answerReferences));
		memset(answerMutexes,       '\0', sizeof(answerMutexes));
	}

	void setAnswerlessConsumer(void (&&consumerProcedureReference) (const _ArgumentType&)) {
		answerlessConsumerProcedureReference = consumerProcedureReference;
	}

	void setAnswerfullConsumer(void (&&consumerProcedureReference) (const _ArgumentType&, _AnswerType*, std::mutex&)) {
		answerfullConsumerProcedureReference = consumerProcedureReference;
	}

	void addListener(void (&&listenerProcedureReference) (const _ArgumentType&)) {
		if (nListenerProcedureReferences >= _NListeners) {
			THROW_EXCEPTION(overflow_error, "Out of listener slots (max="+to_string(_NListeners)+") while attempting to add a new event listener to '" + eventName + "' " +
			                                "(you may wish to increase '_NListeners' at '" + eventName + "'s declaration)");
		}
		listenerProcedureReferences[nListenerProcedureReferences++] = listenerProcedureReference;
	}

	int findListener(void(&&listenerProcedureReference)(const _ArgumentType&)) {
		for (int i=0; i<nListenerProcedureReferences; i++) {
			if (listenerProcedureReferences[i] == listenerProcedureReference) {
				return i;
			}
		}
		return -1;
	}

	bool removeListener(void (&&listenerProcedureReference) (const _ArgumentType&)) {
		int pos = findListener(listenerProcedureReference);
		if (pos == -1) {
			return false;
		}
		memcpy(&(listenerProcedureReferences[pos]), &(listenerProcedureReferences[pos+1]), (nListenerProcedureReferences - (pos+1)) * sizeof(listenerProcedureReferences[0]));
		nListenerProcedureReferences--;
		listenerProcedureReferences[nListenerProcedureReferences] = nullptr;
		return true;
	}

	int findAvailableAnswerSlot() {
		for (int i=0; i<_NAnswers; i++) {
			if (answerReferences[i] == nullptr) {
				return i;
			}
		}
		return -1;
	}
	int reserveAnswerSlot(_AnswerType* answerObjectReference) {
		int eventId = findAvailableAnswerSlot();
		if (eventId == -1) {
			THROW_EXCEPTION(overflow_error, "Out of answer slots (max="+to_string(_NAnswers)+") while attempting to reserve one for a future '" + eventName + "' event answer " +
			                                "(you may wish to increase '_NAnswers' at '" + eventName + "'s declaration)");
		}
		answerReferences[eventId] = answerObjectReference;
		answerMutexes[eventId].try_lock();
		return eventId;
	}
	int reportEvent(const _ArgumentType& eventParameter, _AnswerType* answerObjectReference) {
		int eventId = reserveAnswerSlot(answerObjectReference);
		answerfullConsumerProcedureReference(eventParameter, answerObjectReference, answerMutexes[eventId]);
		return eventId;
	}
	inline void reportEvent(const _ArgumentType& eventParameter) {
		answerlessConsumerProcedureReference(eventParameter);
	}

	_AnswerType* waitForAnswer(size_t eventId) {
		_AnswerType* answer = answerReferences[eventId];
		answerMutexes[eventId].lock();
		answerMutexes[eventId].unlock();
		return answer;
	}

	// not used -- we now pass the mutex to the answerfull consumer
	void reportAnswerIsReady(size_t eventId) {
		answerMutexes[eventId].unlock();
	}

	_AnswerType* reportEventAndWaitForAnswer(const _ArgumentType& eventParameter) {
		static thread_local _AnswerType answer;
		int eventId = reportEvent(eventParameter, &answer);
		return waitForAnswer(eventId);
	}

};

template <typename _AnswerType, typename _ArgumentType, int _NListeners, int _NAnswers>
struct DirectEventLink : EventLink<_AnswerType, _ArgumentType, _NListeners, _NAnswers> {

	_AnswerType* answerObjectReference;
	std::mutex   answerMutex;


	DirectEventLink(string eventName)
			: EventLink<_AnswerType, _ArgumentType, _NListeners, _NAnswers>(eventName)  {}

	inline void notifyEventListeners(const _ArgumentType& eventParameter) {
		for (int i=0; i<this->nListenerProcedureReferences; i++) {
			this->listenerProcedureReferences[i](eventParameter);
		}
	}

	int reportEvent(const _ArgumentType& eventParameter, _AnswerType* answerObjectReference) {
		// sanity check
		if (!this->answerfullConsumerProcedureReference) {
			THROW_EXCEPTION(runtime_error, "Attempting to report an answerfull consumable event '" + this->eventName + "' using a 'DirectEventLink' "
		                                   "before any consumer was registered with 'setAnswerfullConsumer(...)' -- answerfull events must always  "
		                                   "reply, therefore this implementation does not allow not having a consumer method when an answer was "
		                                   "requested (you just called 'reportEvent(argument, answerObjectReference)'). Possible solutions are:\n"
		                                   "    1) Call 'setAnswerfullConsumer(...)' to register a consumer before reporting the answerfull event;\n"
		                                   "    2) Use 'reportEvent(argument)' instead of 'reportEvent(argument, answerObjectReference)'\n"
		                                   "       if you don't mind your event is consumed or not;\n"
		                                   "    3) Use another implementation of 'EventLink' capable of enqueueing events -- which will be\n"
		                                   "       consumed once you register a consumer -- like 'QueueEventLink'.\n");
		}
		// answerfull event consumption
		this->answerObjectReference = answerObjectReference;
		this->answerfullConsumerProcedureReference(eventParameter, answerObjectReference, answerMutex);
		// event notification
		notifyEventListeners(eventParameter);
		return 1;
	}
	inline void reportEvent(const _ArgumentType& eventParameter) {
		// allow answerless event consumption
		if (this->answerlessConsumerProcedureReference) this->answerlessConsumerProcedureReference(eventParameter);
		// event notification
		notifyEventListeners(eventParameter);
	}

	// no wait is needed on this implementation since the answer was already computed at the moment the event got reported with 'reportEvent(eventParameter, answerObjectReference)'
	_AnswerType* waitForAnswer(size_t eventId) {
		return answerObjectReference;
	}

	inline _AnswerType* reportEventAndWaitForAnswer(const _ArgumentType& eventParameter) {
		static thread_local _AnswerType answer;
		reportEvent(eventParameter, &answer);
		return &answer;
	}

};


struct EventLinkSuiteObjects {

    // test case constants
    //static constexpr unsigned int numberOfCalls  = 4'096'000*512;

    // test case data
    //static   std::vector  <std::string>* stdStringKeys;		// keys for all by EASTL containers

    // output messages for boost tests
    static string testOutput;


    EventLinkSuiteObjects() {
    	static bool firstRun = true;
    	if (firstRun) {
			cerr << endl << endl;
			cerr << "Event Link Tests:" << endl;
			cerr << "================ " << endl << endl;
			firstRun = false;
    	}
    }
    ~EventLinkSuiteObjects() {
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
string EventLinkSuiteObjects::testOutput = "";

BOOST_FIXTURE_TEST_SUITE(EventLinkSuiteSuite, EventLinkSuiteObjects);

void _myIShits(int  p)    { cerr << "my  int    shits with p is " << p << endl;}
void _myShits(const int& p)    { cerr << "my  int*   shits with p is " << p << endl;}
void _myShits(const double& p, int* answer, std::mutex& answerMutex) { cerr << "(consuming p=" << p << ") := "; *answer=(p+0.5); answerMutex.unlock();}
void _myListener(const double& p) { cerr << "(listening p=" << p << "); ";}
void _myShits(const float& p)  { cerr << "my float*  shits with p is " << p << endl;}
BOOST_AUTO_TEST_CASE(apiUsageTest) {
	EventFramework<int, MyEvents> myab({
		[(int)MyEvents::EventA]=EventDefinition<int>(_myIShits),
		[(int)MyEvents::EventB]=EventDefinition<int>(_myIShits),
		[(int)MyEvents::EventC]=EventDefinition<int>(_myIShits)});

	EventLink<void, int   , 10, 4>    INT_SHITS("Integral shits");
	INT_SHITS.setAnswerlessConsumer(_myShits);
	EventLink<int, double, 10, 4> DOUBLE_SHITS("Double shits");
	DOUBLE_SHITS.setAnswerfullConsumer(_myShits);
	EventLink<void, float , 10, 4>  FLOAT_SHITS("Floating shits");
	FLOAT_SHITS.setAnswerlessConsumer(_myShits);

	double d=3.14159;
	cerr << "My Output is " << *(DOUBLE_SHITS.reportEventAndWaitForAnswer(d)) << endl;

	DirectEventLink<int, double, 10, 4> DirectEvent("MyDirectEV");
	DirectEvent.setAnswerfullConsumer(_myShits);
	for (int i=0; i<10; i++) {
		DirectEvent.addListener(_myListener);
	}
	for (int i=0; i<9; i++) {
		DirectEvent.removeListener(_myListener);
	}
	cerr << "My Output is " << *(DirectEvent.reportEventAndWaitForAnswer(10.1)) << endl;
}

//BOOST_AUTO_TEST_CASE(apiUsageTest) {
//}

BOOST_AUTO_TEST_SUITE_END();


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
void indirectlyCallableMethod(const int& p1) {
	_r ^= p1;
}

BOOST_FIXTURE_TEST_SUITE(IndirectCallSuite, IndirectCallSuiteObjects);

BOOST_AUTO_TEST_CASE(directCallSpikes) {
	HEAP_MARK();
	int r=19258499;
	_r=1234;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			indirectlyCallableMethod(r);
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("directCallSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("directCallSpikes", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(stdInvokeSpikes) {
	HEAP_MARK();
	int r=19258499;
	_r=1234;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			std::invoke(indirectlyCallableMethod, r);
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("stdInvokeSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("stdInvokeSpikes", output);
	output("r = " + to_string(r) + "\n");
}

template<typename _Argument> struct CallableProcedure {
	void(&&procedureReference)(_Argument);
	CallableProcedure(void(&&procedureReference)(_Argument)) : procedureReference(procedureReference) {}
	inline void invoke(_Argument& argument) {procedureReference(argument);}
};
BOOST_AUTO_TEST_CASE(procedureTemplateSpikes) {
	HEAP_MARK();
	int r=19258499;
	_r=1234;
	CallableProcedure<const int&> procedure(indirectlyCallableMethod);
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			procedure.invoke(r);
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("procedureTemplateSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("procedureTemplateSpikes", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(directEventLinkSpikes) {
	HEAP_MARK();
	int r=19258499;
	_r=1234;
	DirectEventLink<void, int, 10, 4> myEvent("directEventLinkSpikes");
	myEvent.setAnswerlessConsumer(indirectlyCallableMethod);
	//myEvent.addListener(indirectlyCallableMethod);
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			//myEvent.reportEvent(r);
			myEvent.answerlessConsumerProcedureReference(r);
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("directEventLinkSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("directEventLinkSpikes", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(deferedASyncSpikes) {
	HEAP_MARK();
	int r=19258499;
	_r=1234;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls/1000; i++) {
			std::future<void> handle = std::async(std::launch::deferred, indirectlyCallableMethod, r);
			handle.wait();
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("deferedASyncSpikes Pass " + to_string(p) + " execution time: " + to_string((finish - start)*1000) + "µs\n");
	}
	HEAP_TRACE("deferedASyncSpikes (/ 1'000)", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(threadedASyncSpikes) {
	HEAP_MARK();
	int r=19258499;
	_r=1234;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls/10000; i++) {
			std::future<void> handle = std::async(std::launch::async, indirectlyCallableMethod, r);
			handle.wait();
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("threadedASyncSpikes Pass " + to_string(p) + " execution time: " + to_string((finish - start)*10000) + "µs\n");
	}
	HEAP_TRACE("threadedASyncSpikes (/ 10'000)", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(stdFunctionSpikes) {
	HEAP_MARK();
	std::function<void(const int&)> callableObject = indirectlyCallableMethod;
	int r=19258499;
	_r=1234;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls/10; i++) {
			callableObject(r);
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("stdFunctionSpikes Pass " + to_string(p) + " execution time: " + to_string((finish - start)*10) + "µs\n");
	}
	HEAP_TRACE("stdFunctionSpikes (/ 10)", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_SUITE_END();


boost::unit_test::test_suite* init_unit_test_suite(int argc, char* args[]) {
	boost::unit_test::framework::master_test_suite().p_name.value = "EventsFrameworkTests";
	return 0;
}
