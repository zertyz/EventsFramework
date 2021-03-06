#include <iostream>
#include <array>
#include <functional>
#include <cstring>
#include <mutex>
#include <future>
#include <queue>
#include <thread>
#include <initializer_list>
using namespace std;

#include <boost/test/unit_test.hpp>
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE EventsFrameworkTests
#include <boost/test/unit_test.hpp>

#include <MutuaTestMacros.h>
#include <BetterExceptions.h>
#include <TimeMeasurements.h>
using namespace mutua::cpputils;

#include <QueueEventLink.h>
#include <QueueEventDispatcher.h>
//using namespace mutua::events;


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
		if (queueTail == queueHead) {
			queueTail--;
			return false;
		}
		//memcpy(&backingArray[pos], &elementToEnqueue, sizeof(_QueueElement));
		backingArray[pos] = elementToEnqueue;
		return true;
	}

	inline bool dequeue(_QueueElement* dequeuedElement) {
		// check if queue is empty -- the following increment has an implicit modulus operation on 'std::numeric_limits<_QueueSlotsType>::max'
		_QueueSlotsType pos = queueHead++;
		if (pos == queueTail) {
			queueHead--;
			return false;
		}
		//memcpy(dequeuedElement, &backingArray[pos], sizeof(_QueueElement));
		*dequeuedElement = backingArray[pos];
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

	inline bool enqueue(_QueueElement& elementToEnqueue) {
		_QueueSlotsType pos = queueTail++;
		if (queueTail == queueHead) {
			queueTail--;
			return false;
		}
		backingArray[pos] = &elementToEnqueue;
		return true;
	}

	inline bool dequeue(_QueueElement*& dequeuedElementPointer) {
		_QueueSlotsType pos = queueHead++;
		if (pos == queueTail) {
			queueHead--;
			return false;
		}
		dequeuedElementPointer = backingArray[pos];
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
			, queueReservedHead(0)
			, reservations{false} {}

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
};

template <typename _AnswerType, typename _ArgumentType, int _NListeners>
struct DirectEventLink : EventLink<_AnswerType, _ArgumentType, _NListeners> {

	_AnswerType* answerObjectReference;
	std::mutex   answerMutex;


	DirectEventLink(string eventName)
			: EventLink<_AnswerType, _ArgumentType, _NListeners>(eventName)  {}


	inline int reportEvent(const _ArgumentType& eventParameter, _AnswerType* answerObjectReference) {
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
		this->notifyEventListeners(eventParameter);
		return 1;
	}

	inline void reportEvent(const _ArgumentType& eventParameter) {
		// answerless event consumption
		this->answerlessConsumerProcedureReference(eventParameter);
		// event notification
		this->notifyEventListeners(eventParameter);
	}

	// no wait is needed on this implementation since the answer was already computed at the moment the event got reported with 'reportEvent(eventParameter, answerObjectReference)'
	inline _AnswerType* waitForAnswer(int eventId) {
		return answerObjectReference;
	}

	inline _AnswerType* reportEventAndWaitForAnswer(const _ArgumentType& eventParameter) {
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

template <typename _AnswerType, typename _ArgumentType, int _NListeners, typename _QueueSlotsType>
struct QueuedEventLink : EventLink<_AnswerType, _ArgumentType, _NListeners> {

	// answers
	struct AnswerfullEvent {
		_AnswerType*    answerObjectReference;
		_ArgumentType   eventParameter;
		mutex           answerMutex;

		AnswerfullEvent() {}
	};

	// events buffer
	AnswerfullEvent events[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];

	// events queues
	NonBlockingNonReentrantPointerQueue<AnswerfullEvent, _QueueSlotsType> toBeConsumedEvents;
	//NonBlockingNonReentrantPointerQueue<AnswerfullEvent, _QueueSlotsType> toBeNotifyedEvents;

	bool            reservations[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];
	_QueueSlotsType eventIdSequence;

	// mutexes
	mutex  reservationGuard;
	mutex* fullGuard;
	mutex  enqueueGuard;
	mutex  dequeueGuard;
	mutex* emptyGuard;

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
			, reservations{false}
			, eventIdSequence(0) {}

	/** Reserves an 'eventId' (and returns it) for further enqueueing.
	 *  Points 'eventParameterPointer' to a location able to be filled with the event information.
	 *  Returns -1 is an event cannot be reserved. */
	inline int reserveEventForReporting(_ArgumentType*& eventParameterPointer, _AnswerType* answerObjectReference) {

		// find a free event slot 'eventId'
		_QueueSlotsType eventId;
		_QueueSlotsType counter = -1;
		do {
			eventId = eventIdSequence++;
			counter--;
		} while ( (reservations[eventId] != false) && (counter != 0));
		// events buffer are full?
		if (counter == 0) {
			return -1;
		}

		// prepare the event slot and return the event id
		reservations[eventId]              = true;
		AnswerfullEvent& futureEvent       = events[eventId];
		eventParameterPointer              = &futureEvent.eventParameter;
		futureEvent.answerObjectReference  = answerObjectReference;
		// prepare to wait for the answer
		if (answerObjectReference != nullptr) {
			futureEvent.answerMutex.try_lock();
		}
		return eventId;
	}

	inline int reserveEventForReporting(_ArgumentType*& eventParameter) {
		return reserveEventForReporting(eventParameter, nullptr);
	}

	inline int reentrantlyReserveEventForReporting(_ArgumentType*& eventParameterPointer, _AnswerType* answerObjectReference) {

		// find a free event slot 'eventId'
		_QueueSlotsType eventId;
		_QueueSlotsType counter = 0;
		reservationGuard.lock();
		do {
			eventId = eventIdSequence++;
			counter--;
			// slots are full -- wait until 'reentrantlyReleaseSlot(...)' unlocks 'fullGuard'
			if (counter == 0) {
				fullGuard = &reservationGuard;
				reservationGuard.lock();
			}
		} while (reservations[eventId] != false);
		reservationGuard.unlock();

		// prepare the event slot and return the event id
		reservations[eventId]              = true;
		AnswerfullEvent& futureEvent       = events[eventId];
		eventParameterPointer              = &futureEvent.eventParameter;
		futureEvent.answerObjectReference  = answerObjectReference;
		// prepare to wait for the answer
		if (answerObjectReference != nullptr) {
			futureEvent.answerMutex.try_lock();
		}
		return eventId;
	}

	inline int reentrantlyReserveEventForReporting(_ArgumentType& eventParameter) {
		return reentrantlyReserveEventForReporting(eventParameter, nullptr);
	}

	inline void reportReservedEvent(_QueueSlotsType eventId) {
		cerr << "!enqueueing consumer: " << toBeConsumedEvents.enqueue(events[eventId]) << "!" << flush;
		mutex* guard = emptyGuard;
		emptyGuard = nullptr;
		if (guard) {
			guard->unlock();
		}
	}

	inline void reentrantlyReportReservedEvent(_QueueSlotsType eventId) {
		enqueueGuard.lock();
		cerr << "!enqueueing consumer: " << toBeConsumedEvents.enqueue(events[eventId]) << "!" << flush;
		mutex* guard = emptyGuard;
		emptyGuard = nullptr;
		if (guard) {
			guard->unlock();
		}
		enqueueGuard.unlock();
	}

	inline bool dequeue(AnswerfullEvent*& dequeuedElementPointer) {
		return toBeConsumedEvents.dequeue(dequeuedElementPointer);
	}

	inline AnswerfullEvent* reentrantlyDequeue(AnswerfullEvent*& dequeuedElementPointer) {
		dequeueGuard.lock();
		while (true) {
			bool hadElement = toBeConsumedEvents.dequeue(dequeuedElementPointer);
			if (hadElement) {
				break;
			} else {
				// queue is empty -- wait until 'reentrantlyReportReservedEvent(...)' unlocks 'emptyGuard'
				emptyGuard = &dequeueGuard;
				dequeueGuard.lock();
			}
		}
		dequeueGuard.unlock();
		return dequeuedElementPointer;
	}

	inline _QueueSlotsType releaseSlot(AnswerfullEvent*& dequeuedElementPointer) {
		_QueueSlotsType eventId = (dequeuedElementPointer - events);
		reservations[eventId] = false;
		return eventId;
	}

	inline _QueueSlotsType reentrantlyReleaseSlot(AnswerfullEvent*& dequeuedElementPointer) {
		_QueueSlotsType eventId = (dequeuedElementPointer - events);
		reservations[eventId] = false;
		mutex* guard = fullGuard;
		fullGuard = nullptr;
		if (guard) {
			guard->unlock();
		}
		return eventId;
	}

	inline _AnswerType* waitForAnswer(_QueueSlotsType eventId) {
		AnswerfullEvent& event = events[eventId];
		if (event.answerObjectReference == nullptr) {
			THROW_EXCEPTION(runtime_error, "Attempting to wait for an answer from an event of '" + this->eventName + "', which was not prepared to produce an answer. "
		                                   "Did you call 'reserveEventForReporting(_ArgumentType)' instead of 'reserveEventForReporting(_ArgumentType&, const _AnswerType&)' ?");
		}
		event.answerMutex.lock();
		event.answerMutex.unlock();
		return event.answerObjectReference;
	}

	inline void consume(AnswerfullEvent* event) {
		if (event->answerObjectReference != nullptr) {
			this->answerfullConsumerProcedureReference(event->eventParameter, event->answerObjectReference, event->answerMutex);
		} else {
			this->answerlessConsumerProcedureReference(event->eventParameter);
		}
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
/*	EventFramework<int, MyEvents> myab({
		[(int)MyEvents::EventA]=EventDefinition<int>(_myIShits),
		[(int)MyEvents::EventB]=EventDefinition<int>(_myIShits),
		[(int)MyEvents::EventC]=EventDefinition<int>(_myIShits)});

	DirectEventLink<void, int,   10>    INT_SHITS("Integral shits");
	INT_SHITS.setAnswerlessConsumer(_myShits);
	DirectEventLink<int, double, 10> DOUBLE_SHITS("Double shits");
	DOUBLE_SHITS.setAnswerfullConsumer(_myShits);
	DirectEventLink<void, float, 10>  FLOAT_SHITS("Floating shits");
	FLOAT_SHITS.setAnswerlessConsumer(_myShits);

	double d=3.14159;
	cerr << "My Output is " << *(DOUBLE_SHITS.reportEventAndWaitForAnswer(d)) << endl;

	DirectEventLink<int, double, 10> DirectEvent("MyDirectEV");
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

	// queued event links
	QueuedEventLink<int, double, 10, uint_fast8_t> qShits("Queued event double shits");
	QueueEventDispatcher dShits(qShits, 1);
	qShits.addListener(_myListener);
	qShits.setAnswerfullConsumer(_myShits);
	double* reservedParameterReference;
	this_thread::sleep_for(chrono::milliseconds(300));
cerr << "||issueing event||" << flush;
	eventId = qShits.reserveEventForReporting(reservedParameterReference, &answer);
cerr << "||eventId is " << eventId << "||" << flush;
	*reservedParameterReference = 15.49;
	qShits.reportReservedEvent(eventId);
	//QueueEventDispatcher dShits(qShits);
	this_thread::sleep_for(chrono::milliseconds(3000));
	dShits.stopASAP();
	cerr << "Queued Output is " << *qShits.waitForAnswer(eventId) << endl;
*/}

//BOOST_AUTO_TEST_CASE(apiUsageTest) {
//}

BOOST_AUTO_TEST_SUITE_END();


struct QueueEventLinkSuiteObjects {

    // test case constants
    static constexpr unsigned int numberOfCalls  = 4'096'000*512;
    static constexpr unsigned int numberOfPasses = 4;
    static constexpr unsigned int threads        = 4;

    // test case data
    static   std::vector  <std::string>* stdStringKeys;		// keys for all by EASTL containers

    // output messages for boost tests
    static string testOutput;


    QueueEventLinkSuiteObjects()
    		: answerfullConsumedEvents {0}
    		, answerlessConsumedEvents  {0}
    		, notifyedEvents            {0} {
    	static bool firstRun = true;
    	if (firstRun) {
			cerr << endl << endl;
			cerr << "QueueEventLink tests:" << endl;
			cerr << "==================== " << endl << endl;
			firstRun = false;
    	}
    }
    ~QueueEventLinkSuiteObjects() {
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


	mutex m;

    // event consumers & listeners
    atomic_uint answerfullConsumedEvents[65536];
    inline void _answerfullEventConsumer(const unsigned int& n, unsigned int* answer, std::mutex& answerMutex) {
//    	m.lock();
    	answerfullConsumedEvents[n]++;
//    	m.unlock();
    	*answer = n;
    	answerMutex.unlock();	// answer is ready
	}
    atomic_uint answerlessConsumedEvents[65536];
	inline void _answerlessEventConsumer(const unsigned int& n) {
		//m.lock();
    	answerlessConsumedEvents[n]++;
    	//m.unlock();
    	///*if (n%10 == 0)*/ this_thread::sleep_for(chrono::milliseconds(1));
		//cerr << n << ((n%26 == 0) ? ",\n" : ",") << flush;
	}
	atomic_uint notifyedEvents[65536];
	inline void _eventListener1(const unsigned int& n) {
    	notifyedEvents[n]+=1;
	}
	inline void _eventListener2(const unsigned int& n) {
    	notifyedEvents[n]+=2;
	}
	inline void _eventListener4(const unsigned int& n) {
    	notifyedEvents[n]+=4;
	}
	inline void _eventListener8(const unsigned int& n) {
    	notifyedEvents[n]+=8;
	}
	inline void _eventListener16(const unsigned int& n) {
    	notifyedEvents[n]+=16;
	}
	inline void _eventListener32(const unsigned int& n) {
    	notifyedEvents[n]+=32;
	}
	inline void _eventListener64(const unsigned int& n) {
    	notifyedEvents[n]+=64;
	}
	inline void _eventListener128(const unsigned int& n) {
    	notifyedEvents[n]+=128;
	}
	inline void _eventListener256(const unsigned int& n) {
    	notifyedEvents[n]+=256;
	}
	inline void _eventListener512(const unsigned int& n) {
    	notifyedEvents[n]+=512;
	}

	void checkAllElements(atomic_uint* consumerArray, atomic_uint* listenerArray, unsigned int expectedValue) {
		for (int i=0; i<65536; i++) {
			if (consumerArray[i] != expectedValue) {
				BOOST_REQUIRE_MESSAGE(consumerArray[i] == expectedValue, "consumerArray["+to_string(i)+"] == " + to_string(expectedValue) + " failed (it is "+to_string(consumerArray[i])+").");
//				BOOST_REQUIRE(consumerArray[i] == expectedValue);
			}
		}
		for (int i=0; i<65536; i++) {
			if (listenerArray[i] != expectedValue) {
				BOOST_REQUIRE_MESSAGE(listenerArray[i] == expectedValue, "listenerArray["+to_string(i)+"] == " + to_string(expectedValue) + " failed (it is "+to_string(listenerArray[i])+").");
//				BOOST_REQUIRE(listenerArray[i] == expectedValue);
			}
		}
	}

};
// static initializers
string QueueEventLinkSuiteObjects::testOutput = "";


BOOST_FIXTURE_TEST_SUITE(QueueEventLinkSuite, QueueEventLinkSuiteObjects);

BOOST_AUTO_TEST_CASE(testAvailableQueueSlots) {
	HEAP_MARK();

	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 8> queue256("256 slots queue");
	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 7> queue128("128 slots queue");
	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 6> queue64 (" 64 slots queue");
	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 5> queue32 (" 32 slots queue");
	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 4> queue16 (" 16 slots queue");
	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 3> queue8  ("  8 slots queue");
	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 2> queue4  ("  4 slots queue");
	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 1> queue2  ("  2 slots queue");

	output("Testing that we may fill all available queue slots before consuming any event\n");
	output("(if this test blocks on a deadlock, you may consider this test as having failed)\n");

	// test lengths one by one, until queue is full
	output("Length tests: \n");

	// macros
	/////////

#define testReserveThenReport(_numberOfSlots)                                            \
	output("\t" + queue##_numberOfSlots.eventName + " -- (r)eserve, then (R)eport: ");   \
	for (int i=1; i<=_numberOfSlots; i++) {                                              \
		unsigned int* element;                                                           \
		unsigned int eventId = queue##_numberOfSlots.reserveEventForReporting(element);  \
		*element = i;                                                                    \
		output("r");                                                                     \
	}                                                                                    \
	for (int eventId=0; eventId<_numberOfSlots; eventId++) {                             \
		queue##_numberOfSlots.reportReservedEvent(eventId);                              \
		output("R");                                                                     \
	}                                                                                    \
	output("\n");                                                                        \

#define testFullEnqueueing(_numberOfSlots)                                               \
	output("\t" + queue##_numberOfSlots.eventName + " (full enqueueing procress): ");    \
	for (int i=1; i<=_numberOfSlots; i++) {                                              \
		unsigned int* element;                                                           \
		unsigned int eventId = queue##_numberOfSlots.reserveEventForReporting(element);  \
		*element = i;                                                                    \
		queue##_numberOfSlots.reportReservedEvent(eventId);                              \
		output(".");                                                                     \
	}                                                                                    \
	output("\n");                                                                        \

#define consumeEvents(_numberOfSlots)                                                                          \
	output("\t" + queue##_numberOfSlots.eventName + " -- reserveEventFor(D)ispatching, then release(E)vent: ");\
	for (int i=0; i<_numberOfSlots; i++) {                                                                     \
		decltype(queue##_numberOfSlots)::QueueElement* queueElement;                                           \
		unsigned int eId;                                                                                      \
		eId = queue##_numberOfSlots.reserveEventForDispatching(queueElement);                                  \
		output("D");                                                                                           \
		queue##_numberOfSlots.releaseEvent(eId);                                                               \
		output("E");                                                                                           \
	}                                                                                                          \
	output("\n");                                                                                              \


	// uncomment in case of deadlock
//	thread([&] {
//		for (int i=0; i<2; i++) {
//			bool isReservationGuardLocked = !queue256.reservationGuard.try_lock();
//			bool isFullGuardNotNull       =  queue256.fullGuard != nullptr;
//			bool isQueueGuardLocked       = !queue256.queueGuard.try_lock();
//			bool isDequeueGuardLocked     = !queue256.dequeueGuard.try_lock();
//			bool isEmptyGuardNotNull      =  queue256.emptyGuard != nullptr;
//			if (!isReservationGuardLocked)   queue256.reservationGuard.unlock();
//			if (!isQueueGuardLocked)         queue256.queueGuard.unlock();
//			if (!isDequeueGuardLocked)       queue256.dequeueGuard.unlock();
//			cerr << "\nQueueEventDispatcher('" << queue256.eventName << "'): rHead=" << queue256.queueReservedHead << "; rTail=" << queue256.queueReservedTail << "; reservedLength: " << queue256.getQueueReservedLength() << "; ((queueReservedTail+1) & " << queue256.queueSlotsModulus << ")=" << ((queue256.queueReservedTail+1) & queue256.queueSlotsModulus) << "; reservations[queueReservedHead]=" << queue256.events[queue256.queueReservedHead].reserved << "; isReservationGuardLocked=" << isReservationGuardLocked << "; isFullGuardNotNull=" << isFullGuardNotNull << "; isQueueGuardLocked=" << isQueueGuardLocked << "; isDequeueGuardLocked=" << isDequeueGuardLocked << "; isEmptyGuardNotNull=" << isEmptyGuardNotNull << endl << flush;
//			this_thread::sleep_for(chrono::milliseconds(1000));
//		}
//	}).detach();
//	this_thread::sleep_for(chrono::milliseconds(300));

	testReserveThenReport(2);
	testReserveThenReport(4);
	testReserveThenReport(8);
	testReserveThenReport(16);
	testReserveThenReport(32);
	testReserveThenReport(64);
	testReserveThenReport(128);
	testReserveThenReport(256);
	consumeEvents(2);
	consumeEvents(4);
	consumeEvents(8);
	consumeEvents(16);
	consumeEvents(32);
	consumeEvents(64);
	consumeEvents(128);
	consumeEvents(256);

	testFullEnqueueing(2);
	testFullEnqueueing(4);
	testFullEnqueueing(8);
	testFullEnqueueing(16);
	testFullEnqueueing(32);
	testFullEnqueueing(64);
	testFullEnqueueing(128);
	testFullEnqueueing(256);
	consumeEvents(2);
	consumeEvents(4);
	consumeEvents(8);
	consumeEvents(16);
	consumeEvents(32);
	consumeEvents(64);
	consumeEvents(128);
	consumeEvents(256);

#undef testFullEnqueueing
#undef testReserveThenReport
#undef consumeEvents

	HEAP_TRACE("testAvailableQueueSlots", output);
}


BOOST_AUTO_TEST_CASE(testQueueLengthAndQueueReservedLength) {
	HEAP_MARK();

	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 8> queue("testQueueLengthAndQueueReservedLength");

	BOOST_TEST(queue.getQueueLength() == 0,         "Initial queue length should be zero");
	BOOST_TEST(queue.getQueueReservedLength() == 0, "Initial queue reserved length should be zero");

	// test lengths one by one, until queue is full
	output("Length tests: ");
	for (int expectedLength=1; expectedLength<=256; expectedLength++) {
		unsigned int* element;
		unsigned int eventId = queue.reserveEventForReporting(element);
		BOOST_TEST(queue.getQueueReservedLength() == expectedLength);
		*element = expectedLength;
		queue.reportReservedEvent(eventId);
		BOOST_TEST(queue.getQueueLength() == expectedLength);
		output(">");
	}
	// and then back one by one, until it is empty
	for (int expectedLength=255; expectedLength>=0; expectedLength--) {
		decltype(queue)::QueueElement* queueElement;
		unsigned int eventId = queue.reserveEventForDispatching(queueElement);
		BOOST_TEST(queue.getQueueLength() == expectedLength);
		queue.releaseEvent(eventId);
		BOOST_TEST(queue.getQueueReservedLength() == expectedLength);
		output("<");
	}
	output("\n");

	HEAP_TRACE("testQueueLengthAndQueueReservedLength", output);
}

BOOST_AUTO_TEST_CASE(testAnswerfullEvents) {
	HEAP_MARK();

	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 8> myEvent("testAnswerfullEvents");
	myEvent.setAnswerfullConsumer(&QueueEventLinkSuiteObjects::_answerfullEventConsumer, {(QueueEventLinkSuiteObjects*)this, (QueueEventLinkSuiteObjects*)this, (QueueEventLinkSuiteObjects*)this, (QueueEventLinkSuiteObjects*)this});
	myEvent.addListener          (&QueueEventLinkSuiteObjects::_eventListener1,          (QueueEventLinkSuiteObjects*)this);
	mutua::events::QueueEventDispatcher myDispatcher(myEvent, 4, 0, true, true, false, true, true);

	BOOST_TEST(answerfullConsumedEvents[1] == 0);
	BOOST_TEST(          notifyedEvents[1] == 0);

	constexpr int threadsLength = 64;
	thread threads[threadsLength];
	for (int n=0; n<64; n++) {
//		cerr << ">n>" << n << flush;
		// a previous thread already exist on that slot. Lets make sure it has ended.
		if (n >= threadsLength) {
			threads[n%threadsLength].join();
		}
		threads[n%threadsLength] = thread([&] {
			unsigned int* reservedParameterReference;
			unsigned int eventId;
			unsigned int answer;
			for (unsigned int i=0; i<65536; i++) {
				eventId = myEvent.reserveEventForReporting(reservedParameterReference, &answer);
//				cerr << ">>" << eventId << flush;
				*reservedParameterReference = i;
				myEvent.reportReservedEvent(eventId);
//				cerr << " = " << flush;
				myEvent.waitForAnswer(eventId);
//				cerr << i << " (" << answer << ")" << endl << flush;
//				if (i%10 == 0) this_thread::sleep_for(chrono::milliseconds(1));
//				this_thread::sleep_for(chrono::milliseconds(80-n));
			}
		});
	};

	// wait for producers
	for (int n=0; n<threadsLength; n++) {
		threads[n].join();
	}
	//this_thread::sleep_for(chrono::milliseconds(2000));

	// wait until all queue is processed
	myDispatcher.stopWhenEmpty();

	BOOST_TEST(answerfullConsumedEvents[1] == 64);
	BOOST_TEST(          notifyedEvents[1] == 64);
	checkAllElements(answerfullConsumedEvents, notifyedEvents, 64);

	HEAP_TRACE("testAnswerfullEvents", output);
}

BOOST_AUTO_TEST_CASE(waitForAConsumableEvent) {
	HEAP_MARK();

	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 8> qShits("waitForAConsumableEvent");
	qShits.setAnswerlessConsumer(&QueueEventLinkSuiteObjects::_answerlessEventConsumer, {(QueueEventLinkSuiteObjects*)this});

	this_thread::sleep_for(chrono::milliseconds(300));

	BOOST_TEST(answerlessConsumedEvents[1] == 0, "Waiting for event consumption");
	BOOST_TEST(          notifyedEvents[1] == 0, "Waiting for event notification");

	unsigned int* reservedParameterReference;
	unsigned int eventId = qShits.reserveEventForReporting(reservedParameterReference);
	*reservedParameterReference = 1;
	qShits.reportReservedEvent(eventId);

	decltype(qShits)::QueueElement* dequeuedEvent;
	unsigned int dispatchedEventId = qShits.reserveEventForDispatching(dequeuedEvent);
	qShits.answerlessConsumerProcedureReference(qShits.answerlessConsumerThese[0], dequeuedEvent->eventParameter);
	qShits.releaseEvent(dispatchedEventId);


	this_thread::sleep_for(chrono::milliseconds(10));

	BOOST_TEST(answerlessConsumedEvents[1] == 1,    "Event consumption");

	HEAP_TRACE("waitForAConsumableEvent", output);
}

BOOST_AUTO_TEST_CASE(busyEventGeneration) {
	HEAP_MARK();

	mutua::events::QueueEventLink<unsigned int, unsigned int, 10, 8> myEvent("busyEventGeneration tests");
	myEvent.setAnswerlessConsumer(&QueueEventLinkSuiteObjects::_answerlessEventConsumer, {(QueueEventLinkSuiteObjects*)this, (QueueEventLinkSuiteObjects*)this, (QueueEventLinkSuiteObjects*)this, (QueueEventLinkSuiteObjects*)this, (QueueEventLinkSuiteObjects*)this});
	myEvent.addListener          (&QueueEventLinkSuiteObjects::_eventListener1,          (QueueEventLinkSuiteObjects*)this);
	mutua::events::QueueEventDispatcher myDispatcher(myEvent, 5, 0, true, true, true, false, true);

	BOOST_TEST(answerlessConsumedEvents[1] == 0);
	BOOST_TEST(          notifyedEvents[1] == 0);

	constexpr int threadsLength = 4;
	thread threads[threadsLength];
	for (int n=0; n<64; n++) {
//		cerr << ">n>" << n << flush;
		// a previous thread already exist on that slot. Lets make sure it has ended.
		if (n >= threadsLength) {
			threads[n%threadsLength].join();
		}
		threads[n%threadsLength] = thread([&] {
			unsigned int eventId;
			unsigned int* reservedParameterReference;
			for (unsigned int i=0; i<65536; i++) {
				eventId = myEvent.reserveEventForReporting(reservedParameterReference);
//				cerr << ">>" << eventId << flush;
				*reservedParameterReference = i;
				myEvent.reportReservedEvent(eventId);
//				cerr << " = " << i << endl << flush;
//				if (i%10 == 0) this_thread::sleep_for(chrono::milliseconds(1));
				//this_thread::sleep_for(chrono::milliseconds(16));
			}
		});
	}

	// wait for producers
	for (int n=0; n<threadsLength; n++) {
		threads[n].join();
	}
	//this_thread::sleep_for(chrono::milliseconds(2000));

	// wait until all queue is processed
	myDispatcher.stopWhenEmpty();

	BOOST_TEST(answerlessConsumedEvents[1] == 64);
	BOOST_TEST(          notifyedEvents[1] == 64);
	checkAllElements(answerlessConsumedEvents, notifyedEvents, 64);

	HEAP_TRACE("busyEventGeneration", output);
}

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
		output("NonBlockingNonReentrantQueueSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("NonBlockingNonReentrantQueueSpikes", output);
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
		output("stdQueueSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("stdQueueSpikes", output);
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

BOOST_AUTO_TEST_CASE(lambdaCallSpikes) {
	HEAP_MARK();
	int r=19258499;
	_r=1234;
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			[&](){_r ^= r;}();
			r ^= _r;
		}
		unsigned long long finish = TimeMeasurements::getMonotonicRealTimeUS();
		output("lambdaCallSpikes Pass " + to_string(p) + " execution time: " + to_string(finish - start) + "µs\n");
	}
	HEAP_TRACE("lambdaCallSpikes", output);
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
	DirectEventLink<void, int, 10> myEvent("directEventLinkSpikes");
	myEvent.setAnswerlessConsumer(indirectlyCallableMethod);
	//myEvent.addListener(indirectlyCallableMethod);
	for (int p=0; p<numberOfPasses; p++) {
		unsigned long long start = TimeMeasurements::getMonotonicRealTimeUS();
		for (unsigned int i=0; i<numberOfCalls; i++) {
			myEvent.reportEvent(r);
			//myEvent.answerlessConsumerProcedureReference(r);
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
