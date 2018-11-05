#include <iostream>
#include <array>
#include <functional>
#include <cstring>
#include <mutex>
#include <future>
#include <queue>
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

template <typename _AnswerType, typename _ArgumentType, int _NListeners>
struct EventLink {


	// default consumers
	static void defaultAnswerlessConsumer (const _ArgumentType& p) {
		// might enqueue requests on a {argument} list
	}
	static void defaultAnswerfullConsumer (const _ArgumentType& p, _AnswerType* a, std::mutex& m) {
		// might enqueue requests on a {argument, answer, mutex} list
	}


	// debug info
	string eventName;

	// consumer
	void (*answerlessConsumerProcedureReference) (const _ArgumentType&);
	void (*answerfullConsumerProcedureReference) (const _ArgumentType&, _AnswerType*, std::mutex&);

	// listeners
	void (*listenerProcedureReferences[_NListeners]) (const _ArgumentType&);
	int nListenerProcedureReferences;


	EventLink(string eventListenerName)
			: eventName(eventListenerName)
			, answerlessConsumerProcedureReference(defaultAnswerlessConsumer)
			, answerfullConsumerProcedureReference(defaultAnswerfullConsumer) {
		// initialize listeners
		memset(listenerProcedureReferences, '\0', sizeof(listenerProcedureReferences));
		nListenerProcedureReferences = 0;
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

	inline void notifyEventListeners(const _ArgumentType& eventParameter) {
		for (int i=0; i<this->nListenerProcedureReferences; i++) {
			this->listenerProcedureReferences[i](eventParameter);
		}
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
	inline virtual int reportEvent(const _ArgumentType& eventParameter, _AnswerType* answerObjectReference) {
		int eventId = reserveAnswerSlot(answerObjectReference);
		answerfullConsumerProcedureReference(eventParameter, answerObjectReference, answerMutexes[eventId]);
		return eventId;
	}
	inline virtual void reportEvent(const _ArgumentType& eventParameter) {
		answerlessConsumerProcedureReference(eventParameter);
	}

	inline virtual _AnswerType* waitForAnswer(int eventId) {
		_AnswerType* answer = answerReferences[eventId];
		answerMutexes[eventId].lock();
		answerMutexes[eventId].unlock();
		return answer;
	}

	// not used -- we now pass the mutex to the answerfull consumer
	void reportAnswerIsReady(int eventId) {
		answerMutexes[eventId].unlock();
	}

	inline virtual _AnswerType* reportEventAndWaitForAnswer(const _ArgumentType& eventParameter) = 0;

};

template <typename _AnswerType, typename _ArgumentType, int _NListeners>
struct DirectEventLink : EventLink<_AnswerType, _ArgumentType, _NListeners> {

	_AnswerType* answerObjectReference;
	std::mutex   answerMutex;


	DirectEventLink(string eventName)
			: EventLink<_AnswerType, _ArgumentType, _NListeners, _NAnswers>(eventName)  {}


	inline int reportEvent(const _ArgumentType& eventParameter, _AnswerType* answerObjectReference) override {
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

	inline void reportEvent(const _ArgumentType& eventParameter) override {
		// answerless event consumption
		this->answerlessConsumerProcedureReference(eventParameter);
		// event notification
		notifyEventListeners(eventParameter);
	}

	// no wait is needed on this implementation since the answer was already computed at the moment the event got reported with 'reportEvent(eventParameter, answerObjectReference)'
	inline _AnswerType* waitForAnswer(int eventId) override {
		return answerObjectReference;
	}

	inline _AnswerType* reportEventAndWaitForAnswer(const _ArgumentType& eventParameter) override {
		if constexpr (std::is_same<_AnswerType, void>::value) {
			// method body when _AnswerType is void
			this->answerfullConsumerProcedureReference(eventParameter, nullptr, answerMutex);
			return nullptr;
		} else {
			// method body when _AnswerType isn't void
			static thread_local _AnswerType answer;
			reportEvent(eventParameter, &answer);
			return &answer;
		}
	}

};

template <typename _QueueElement, typename _QueueSlotsType = uint_fast8_t>
struct NonBlockingNonReentrantZeroCopyQueue;

template <typename _AnswerType, typename _ArgumentType, int _NListeners, typename _QueueSlotsType>
struct QueuedEventLink : EventLink<_AnswerType, _ArgumentType, _NListeners> {

	// answers
	struct AnswerfullEvent {
		_AnswerType*    answerObjectReference;
		_ArgumentType   eventParameter;
		std::mutex      answerMutex;
	};

	// events buffer
	AnswerfullEvent events[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];

	// events queues
	NonBlockingNonReentrantPointerQueue<AnswerfullEvent,   _QueueSlotsType> toBeConsumedEvents;
	NonBlockingNonReentrantPointerQueue<_ArgumentType, _QueueSlotsType>     toBeNotifyedEvents;

	_QueueSlotsType eventsReferenceCounters[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];
	_QueueSlotsType eventIdSequence;

	/* PARA AMANHÃ:
	 *
	 * x1) Eventos são consumidos nas duas filas acima, porém a fila dos que demandam resposta têm prioridade
	 * x2) Os consumidores que demandam resposta não precisam dos dois arrays abaixo (respostas & mutexes). Ao contrário,
	 *    a fila de resposta será um struct {eventParameter, answerReference, mutex}
	 * 3) Os listeners são despachados por outra(s) thread(s), possivelmente independentes? ou pela(s) thread(s) do(s)
	 *    consumidor(es), porém com prioridade ainda menor que a dos consumidores? Ou é possível escolher entre ambos?
	 * 4) Eventos não são mais consumidos ou notificados por threads presentes aqui. Uma outra entidade, possivelmente
	 *    chamada 'EventDispatcher', coordena a prioridade e número de threads dos consumidores e observadores.
	 *    Algo assim: EventDispatcher{{{1, REAL_TIME}, {ReadDiskFile, -1}}, {{4, NORMAL}, {ProcessBackend, -1}, {UpdateCache, -2}}
	 *    Agrupados estão os eventos UpdateCache e ProcessBackend para serem, ambos, processados por 4 Threads com prioridade normal, sendo que o ProcessBackend é mais prioritário: 1 UpdateCache só pode ser executado depois de dois ProcessBackends ou se não houverem ProcessBackends na fila
	 *    Outro grupo é ReadDiskFile, contendo apenas 1 thread real time e um evento, ReadDiskFile. A prioridade aqui não precisaria ser especificada, visto que só há eles neste thread group
	 *    Estas são as threads para os consumers. Faltou aqui especificar as threads para os listeners

	*/


	QueuedEventLink(string eventName)
			: EventLink<_AnswerType, _ArgumentType, _NListeners>(eventName)
			, eventIdSequence(0) {}

	/** Reserves an 'eventId' (and returns it) for further enqueueing.
	 *  Points 'eventParameterPointer' to a location able to be filled with the event information.
	 *  Returns -1 is an event cannot be reserved. */
	inline int reserveEventForReporting(_ArgumentType& eventParameterPointer, const _AnswerType& answerObjectReference) {
		// find a free event slot 'eventId'
		_QueueSlotsType eventId;
		_QueueSlotsType counter = -1;
		while ( eventsReferenceCounters[eventIdSequence] != 0 ) {
			// events buffer are full?
			if (counter == 0) {
				return -1;
			}
			eventId = eventIdSequence++;
			counter--;
		}
		// prepare the event slot and return the event id
		eventsReferenceCounters[eventId]++;		// increment the event's reference counter, marking this slot as 'not available until consumed'
		AnswerfullEvent& futureEvent       = events[eventId];
		eventParameterPointer              = &futureEvent.eventParameter;
		futureEvent.answerObjectReference  = answerObjectReference;
		// prepare to wait for the answer
		if (answerObjectReference != nullptr) {
			futureEvent.answerMutex.try_lock();
		}
		return eventId;
	}

	inline int reserveEventForReporting(_ArgumentType& eventParameter) {
		return reserveEventForReporting(eventParameter, nullptr);
	}

	inline void reportReservedEvent(_QueueSlotsType eventId) {
		toBeConsumedEvents.enqueue(events[events]);
		// do we have listeners?
		if (this->nListenerProcedureReferences > 0) {
			toBeNotifyedEvents.enqueue(events[events]);
			eventsReferenceCounters[eventId]++;	// increment further the event's reference counter, marking this slot as 'not available until listened' as well
		}
	}

	inline _AnswerType* waitForAnswer(int eventId) override {
		AnswerfullEvent& event = events[eventId];
		if (event.answerObjectReference == nullptr) {
			THROW_EXCEPTION(runtime_error, "Attempting to wait for an answer from an event of '" + this->eventName + "', which was not prepared to produce an answer. "
		                                   "Did you call 'reserveEventForReporting(_ArgumentType)' instead of 'reserveEventForReporting(_ArgumentType&, const _AnswerType&)' ?");
		}
		event.answerMutex.lock();
		event.answerMutex.unlock();
		return event.answerObjectReference;
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

	DirectEventLink<void, int   , 10, 4>    INT_SHITS("Integral shits");
	INT_SHITS.setAnswerlessConsumer(_myShits);
	DirectEventLink<int, double, 10, 4> DOUBLE_SHITS("Double shits");
	DOUBLE_SHITS.setAnswerfullConsumer(_myShits);
	DirectEventLink<void, float , 10, 4>  FLOAT_SHITS("Floating shits");
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

	int answer;
	int eventId = DirectEvent.reportEvent(12.49, &answer);
	cerr << "My delayed output is ";
	DirectEvent.waitForAnswer(eventId);
	cerr << answer << endl;
}

//BOOST_AUTO_TEST_CASE(apiUsageTest) {
//}

BOOST_AUTO_TEST_SUITE_END();


struct QueueSuiteObjects {

    // test case constants
    static constexpr unsigned int numberOfCalls  = 4'096'000*512;
    static constexpr unsigned int numberOfPasses = 4;
    static constexpr unsigned int threads        = 4;

    // test case data
    static   std::vector  <std::string>* stdStringKeys;		// keys for all by EASTL containers

    // output messages for boost tests
    static string testOutput;


    QueueSuiteObjects() {
    	static bool firstRun = true;
    	if (firstRun) {
			cerr << endl << endl;
			cerr << "Queue Spikes:" << endl;
			cerr << "============ " << endl << endl;
			firstRun = false;
    	}
    }
    ~QueueSuiteObjects() {
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
string QueueSuiteObjects::testOutput = "";


template <typename _QueueElement, typename _QueueSlotsType = uint_fast8_t>
struct NonBlockingNonReentrantQueue {

	_QueueElement backingArray[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];	// an array sized like this allows implicit modulus operations on indexes of the same type (_QueueSlotsType)
	_QueueSlotsType queueHead;
	_QueueSlotsType queueTail;

	NonBlockingNonReentrantQueue() {
		queueHead = 0;
		queueTail = 0;
	}

	// TODO another constructor might receive pointers to all local variables -- allowing for mmapped backed queues

	inline bool enqueue(_QueueElement& elementToEnqueue) {
		// check if queue is full -- the following increment has an implicit modulus operation on 'std::numeric_limits<_QueueSlotsType>::max'
		_QueueSlotsType pos = queueTail++;
		if (pos == queueHead) {
			queueTail--;
			return false;
		}
		memcpy(&backingArray[pos], &elementToEnqueue, sizeof(_QueueElement));
		return true;
	}

	inline bool dequeue(_QueueElement* dequeuedElement) {
		// check if queue is empty -- the following increment has an implicit modulus operation on 'std::numeric_limits<_QueueSlotsType>::max'
		_QueueSlotsType pos = queueHead++;
		if (pos == queueTail) {
			queueHead--;
			return false;
		}
		memcpy(dequeuedElement, &backingArray[pos], sizeof(_QueueElement));
		return true;
	}
};

template <typename _QueueElement, typename _QueueSlotsType = uint_fast8_t>
struct NonBlockingNonReentrantPointerQueue {

	_QueueSlotsType queueHead;
	_QueueSlotsType queueTail;
	_QueueElement* backingArray[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];	// an array sized like this allows implicit modulus operations on indexes of the same type (_QueueSlotsType)

	NonBlockingNonReentrantPointerQueue()
			: queueHead(0)
			, queueTail(0) {}

	// TODO another constructor might receive pointers to all local variables -- allowing for mmapped backed queues

	inline bool enqueue(const _QueueElement& elementToEnqueue) {
		_QueueSlotsType pos = queueTail++;
		if (pos == queueHead) {
			queueTail--;
			return false;
		}
		backingArray[pos] = elementToEnqueue;
		return true;
	}

	inline bool dequeue(_QueueElement& dequeuedElement) {
		_QueueSlotsType pos = queueHead++;
		if (pos == queueTail) {
			queueHead--;
			return false;
		}
		dequeuedElement = backingArray[pos];
		return true;
	}
};

template <typename _QueueElement, typename _QueueSlotsType = uint_fast8_t>
struct NonBlockingNonReentrantZeroCopyQueue {

	_QueueSlotsType queueHead;			// will never be behind of 'queueReservedHead'
	_QueueSlotsType queueTail;			// will never be  ahead of 'queueReservedTail'
	_QueueSlotsType queueReservedHead;	// will never be  ahead of 'queueHead'
	_QueueSlotsType queueReservedTail;	// will never be behind of 'queueTail'
	bool          reservations[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];	// keeps track of the conceded but not yet enqueued & conceded but not yet dequeued positions
	_QueueElement backingArray[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];	// an array sized like this allows implicit modulus operations on indexes of the same type (_QueueSlotsType)

	NonBlockingNonReentrantZeroCopyQueue()
			: queueHead        (0)
			, queueTail        (0)
			, queueReservedTail(0)
			, queueReservedHead(0) {}

	// TODO another constructor might receive pointers to all local variables -- allowing for mmapped backed queues

	/** points 'slotPointer' to a reserved queue position, for later enqueueing. When using this 'zero-copy' enqueueing method, do as follows:
	 *    _QueueElement* element;
	 *    if (reserveForEnqueue(element)) {
	 *    	... (fill 'element' with data) ...
	 *    	enqueueReservedSlot(element);
	 *    } */
	inline bool reserveForEnqueue(_QueueElement& slotPointer) {
		_QueueSlotsType reservedPos = queueReservedTail++;
		if (reservedPos == queueReservedHead) {
			queueReservedTail--;
			return false;
		}
		reservations[reservedPos] = true;
		slotPointer = &backingArray[reservedPos];
		return true;
	}

	inline void enqueueReservedSlot(const _QueueElement& slotPointer) {
		_QueueSlotsType reservedPos = slotPointer-backingArray;
		reservations[reservedPos] = false;
		// walk with 'queueTail' for contiguous elements that had already concluded their reservations -- allowing them to be dequeued
		while ( (!reservations[queueTail]) && (queueTail != queueReservedTail) ) {
			queueTail++;
		}
	}

	inline bool reserveForDequeue(_QueueElement& slotPointer) {
		_QueueSlotsType reservedPos = queueHead++;
		if (reservedPos == queueReservedTail) {
			queueHead--;
			return false;
		}
		reservations[reservedPos] = true;
		slotPointer = &backingArray[reservedPos];
		return true;
	}

	inline void dequeueReservedSlot(const _QueueElement& slotPointer) {
		_QueueSlotsType reservedPos = slotPointer-backingArray;
		reservations[reservedPos] = false;
		while ( (!reservations[queueReservedHead]) && (queueReservedHead != queueHead) ) {
			queueReservedHead++;
		}
	}
};


BOOST_FIXTURE_TEST_SUITE(QueueSuite, QueueSuiteObjects);

BOOST_AUTO_TEST_CASE(NonBlockingNonReentrantQueueSpikes) {
	HEAP_MARK();
	int r=19258499;
	int dequeuedR;
	NonBlockingNonReentrantQueue<int> myQueue;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			myQueue.enqueue(r);
			myQueue.dequeue(&dequeuedR);
			r ^= dequeuedR;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("directCallSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("directCallSpikes", output);
	output("r = " + to_string(r) + "\n");
}

BOOST_AUTO_TEST_CASE(stdQueueSpikes) {
	HEAP_MARK();
	int r=19258499;
	int dequeuedR;
	std::queue<int> stdQueue;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			stdQueue.push(r);
			dequeuedR = stdQueue.front();
			stdQueue.pop();
			r ^= dequeuedR;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("directCallSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("directCallSpikes", output);
	output("r = " + to_string(r) + "\n");
}

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
