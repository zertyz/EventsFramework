#ifndef MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_
#define MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_

#include <iostream>
#include <thread>
#include <mutex>
#include <pthread.h>
#include <type_traits>
#include <stdexcept>
using namespace std;

#include <BetterExceptions.h>
using namespace mutua::cpputils;


namespace mutua::events {

	/**
     * QueueEventDispatcher.h
     * ======================
     * created (in C++) by luiz, Nov 5, 2018
     *
     * Responsible for dispatching events to consumers and observers.
     *
     * Implementation of 'EventDispatcher' specialized to deal with 'QueueEventLinks'.
     *
    */
	template <class _QueueEventLink>
	struct QueueEventDispatcher {

		// types from _QueueEventLink:
		typedef  decltype(_QueueEventLink::QueueElement::answerObjectReference) _AnswerTypePointer;
		typedef std::remove_pointer<_AnswerTypePointer>                         _AnswerType;
		typedef decltype(_QueueEventLink::QueueElement::eventParameter)         _ArgumentType;

		bool             isActive;
		_QueueEventLink& el;
		int              nThreads;
		thread*          threads;

		string (*eventParameterToStringSerializer) (const _ArgumentType&);

		QueueEventDispatcher(_QueueEventLink& el,
		                     int              nThreads,
		                     int              threadsPriority,
							 bool             zeroCopy,
							 bool             notifyEvents,
							 bool             consumeAnswerlessEvents,
							 bool             consumeAnswerfullEvents)
				: isActive(true)
				, el(el)
				, nThreads(nThreads/*+1*/) {

			// checks
			if (threadsPriority != 0) {
                THROW_EXCEPTION(invalid_argument, "QueueEventDispatcher: Attempting to create a dispatcher for event '"+el.eventName+"' with custom " +
                                                  "'threadsPriority', and this is not implemented yet -- it must be zero in the meantime.");
			}
			if ( consumeAnswerlessEvents && (nThreads > el.nAnswerlessConsumerThese) ) {
				THROW_EXCEPTION(runtime_error, "QueueEventDispatcher: Attempting to create a dispatcher for event '"+el.eventName+"' with "+to_string(nThreads)+" threads, " +
                                               "but the given QueueEventLink is set to have only "+to_string(el.nAnswerlessConsumerThese)+" consumer objects on the instance pool " +
                                               "and this combination is not optimal. Please, arranje that -- most probably by increasing the array of objects given to 'setAnswerlessConsumer(...)'");
			}
			if ( consumeAnswerfullEvents && (nThreads > el.nAnswerfullConsumerThese) ) {
				THROW_EXCEPTION(runtime_error, "QueueEventDispatcher: Attempting to create a dispatcher for event '"+el.eventName+"' with "+to_string(nThreads)+" threads, " +
                                               "but the given QueueEventLink is set to have only "+to_string(el.nAnswerfullConsumerThese)+" consumer objects on the instance pool " +
                                               "and this combination is not optimal. Please, arranje that -- most probably by increasing the array of objects given to 'setAnswerfullConsumer(...)'");
			}

			threads = new thread[nThreads/*+1*/];

			for (int i=0; i<nThreads; i++) {
				/**/ if ( zeroCopy &&  notifyEvents &&  consumeAnswerlessEvents && !consumeAnswerfullEvents )
					threads[i] = thread(&QueueEventDispatcher::dispatchZeroCopyListeneableAndConsumableAnswerlessEventsLoop, this, i, el.answerlessConsumerThese[i%el.nAnswerlessConsumerThese]);
				else if ( zeroCopy &&  notifyEvents && !consumeAnswerlessEvents &&  consumeAnswerfullEvents )
					threads[i] = thread(&QueueEventDispatcher::dispatchZeroCopyListeneableAndConsumableAnswerfullEventsLoop, this, i, el.answerfullConsumerThese[i%el.nAnswerlessConsumerThese]);
				else if ( zeroCopy && !notifyEvents &&  consumeAnswerlessEvents && !consumeAnswerfullEvents )
					threads[i] = thread(&QueueEventDispatcher::dispatchZeroCopyConsumableAnswerlessEventsLoop,               this, i, el.answerlessConsumerThese[i%el.nAnswerlessConsumerThese]);
				else if ( zeroCopy && !notifyEvents && !consumeAnswerlessEvents &&  consumeAnswerfullEvents )
					threads[i] = thread(&QueueEventDispatcher::dispatchZeroCopyConsumableAnswerfullEventsLoop,               this, i, el.answerfullConsumerThese[i%el.nAnswerlessConsumerThese]);
				else if ( zeroCopy &&  notifyEvents && !consumeAnswerlessEvents && !consumeAnswerfullEvents )
					threads[i] = thread(&QueueEventDispatcher::dispatchZeroCopyListeneableEventsLoop,                        this, i);
				else
	                THROW_EXCEPTION(invalid_argument, "QueueEventDispatcher: Attempting to create a dispatcher for event '"+el.eventName+"' with a not implemented combination of " +
	                                                       "'zeroCopy' (" +                to_string(zeroCopy)+"), " +
	                                                       "'notifyEvents' (" +            to_string(notifyEvents)+"), " +
	                                                       "'consumeAnswerlessEvents' (" + to_string(consumeAnswerlessEvents)+") and " +
	                                                       "'consumeAnswerfullEvents' (" + to_string(consumeAnswerfullEvents)+")");
			}
			/*threads[nThreads] = thread(&QueueEventDispatcher::debug, this);*/

			setArgumentSerializer();
		}

		//static string defaultEventParameterToStringSerializer(         int& argument) { return to_string(argument); }
		static string defaultEventParameterToStringSerializer(const unsigned int& argument) { return to_string(argument); }

		void setArgumentSerializer() {
			if constexpr (std::is_integral<_ArgumentType>::value) {
				eventParameterToStringSerializer = defaultEventParameterToStringSerializer;
			} else if (std::is_class<_ArgumentType>::value) {
				eventParameterToStringSerializer = _ArgumentType::toString;
			} else {
                THROW_EXCEPTION(invalid_argument, "QueueEventDispatcher: Don't know how to serialize the given _ArgumentType for event '"+el.eventName+"'.");
			}
		}

		~QueueEventDispatcher() {
			stopASAP();
			delete[] threads;
		}

		inline bool isMutexLocked(mutex& m) {
			bool isLocked = !m.try_lock();
			if (!isLocked) m.unlock();
			return isLocked;
		}

		void stopASAP() {
			if (isActive) {
				for (int i=0; i<nThreads; i++) {
					threads[i].detach();
				}
				isActive = false;
			}
		}

		inline void consumeAnswerlessEvent(
				unsigned int        threadId,
				void                (*consumerMethod) (void*, const _ArgumentType&),
				void*                consumerThis,
				const _ArgumentType& eventParameter) {
			try {
				consumerMethod(consumerThis, eventParameter);
			} catch (const exception& e) {
				DUMP_EXCEPTION(runtime_error("Exception in answerless consumer: "s + e.what()),
						       "QueueEventDispatcher for event '"+el.eventName+"', thread #"+to_string(threadId)+": exception in answerless consumer " +
					           "with parameter: "+eventParameterToStringSerializer(eventParameter)+". Event consumption will not be retried, " +
					           "since a fall-back queue is not yet implemented.\n" +
				               "Caused by: "+e.what(),
				               "threadId",       to_string(threadId),
				               "consumerMethod", to_string((size_t)consumerMethod),
				               "consumerThis",   to_string((size_t)consumerThis),
				               "eventParameter", eventParameterToStringSerializer(eventParameter));
			} catch (...) {
				DUMP_EXCEPTION(runtime_error("Unknown exception in answerless consumer"),
				               "QueueEventDispatcher for event '"+el.eventName+"', thread #"+to_string(threadId)+": exception in answerless consumer " +
				               "with parameter: "+eventParameterToStringSerializer(eventParameter)+". Event consumption will not be retried, " +
				               "since a fall-back queue is not yet implemented.\n" +
				               "Caused by: <<unknown cause>>",
				               "threadId",       to_string(threadId),
				               "consumerMethod", to_string((size_t)consumerMethod),
				               "consumerThis",   to_string((size_t)consumerThis),
				               "eventParameter", eventParameterToStringSerializer(eventParameter));
			}
		}

		inline void consumeAnswerfullEvent(
				unsigned int                               threadId,
//				void                                     (*consumerMethod) (void*, const _ArgumentType&, _AnswerType*, std::mutex&),
				decltype(_QueueEventLink::answerfullConsumerProcedureReference) consumerMethod,
				void*                                      consumerThis,
				typename _QueueEventLink::QueueElement*    dequeuedEvent) {
			try {
				consumerMethod(consumerThis, dequeuedEvent->eventParameter, dequeuedEvent->answerObjectReference, dequeuedEvent->answerMutex);
			} catch (const exception& e) {
				dequeuedEvent->exception = std::current_exception();
				DUMP_EXCEPTION(runtime_error("Exception in answerfull consumer: "s + e.what()),
				               "QueueEventDispatcher for event '"+el.eventName+"', thread #"+to_string(threadId)+": exception in answerfull consumer " +
				               "with parameter: "+eventParameterToStringSerializer(dequeuedEvent->eventParameter)+". Event consumption will not be retried, " +
				               "since a fall-back queue is not yet implemented.\n" +
				               "Caused By: "+e.what(),
				               "threadId",              to_string(threadId),
				               "consumerMethod",        to_string((size_t)consumerMethod),
				               "consumerThis",          to_string((size_t)consumerThis),
				               "answerObjectReference", to_string((size_t)dequeuedEvent->answerObjectReference),
				               "eventParameter",        eventParameterToStringSerializer(dequeuedEvent->eventParameter));
			} catch (...) {
				dequeuedEvent->exception = std::current_exception();
				DUMP_EXCEPTION(runtime_error("Unknown exception in answerless consumer"),
				               "QueueEventDispatcher for event '"+el.eventName+"', thread #"+to_string(threadId)+": exception in answerfull consumer " +
				               "with parameter: "+eventParameterToStringSerializer(dequeuedEvent->eventParameter)+". Event consumption will not be retried, " +
				               "since a fall-back queue is not yet implemented.\n" +
				               "Caused By: <<unknown cause>>",
				               "threadId",              to_string(threadId),
				               "consumerMethod",        to_string((size_t)consumerMethod),
				               "consumerThis",          to_string((size_t)consumerThis),
				               "answerObjectReference", to_string((size_t)dequeuedEvent->answerObjectReference),
				               "eventParameter",        eventParameterToStringSerializer(dequeuedEvent->eventParameter));

				// prepare the exeption to be visible when the caller issues an 'waitForAnswer'
				if (isMutexLocked(dequeuedEvent->answerMutex)) {
					// the exception happened before the answer was issued
					dequeuedEvent->answerObjectReference = nullptr;
					dequeuedEvent->answerMutex.unlock();
				}
			}
		}

		inline void notifyEventObservers(
				unsigned int         threadId,
				void              (**listenerMethods) (void*, const _ArgumentType&),
				void*               *listenersThis,
				const _ArgumentType& eventParameter) {
			for (unsigned int i=0; i<el.nListenerProcedureReferences; i++) try {
				listenerMethods[i](listenersThis[i], eventParameter);
			} catch (const exception& e) {
				DUMP_EXCEPTION(runtime_error("Exception in listener: "s + e.what()),
				               "QueueEventDispatcher for event '"+el.eventName+"', thread #"+to_string(threadId)+": exception in event listener #"+to_string(i)+
				               "with parameter: "+eventParameterToStringSerializer(eventParameter)+".\n" +
				               "Caused by: "+e.what(),
				               "threadId",                         to_string(threadId),
				               "listenerMethod["+to_string(i)+"]", to_string((size_t)listenerMethods[i]),
				               "listenerThis["+to_string(i)+"]",   to_string((size_t)listenersThis[i]),
				               "eventParameter",                   eventParameterToStringSerializer(eventParameter));
			} catch (...) {
				std::exception_ptr e = std::current_exception();
				DUMP_EXCEPTION(runtime_error("Unknown Exception in listener"),
				               "QueueEventDispatcher for event '"+el.eventName+"', thread #"+to_string(threadId)+": exception in event listener #"+to_string(i)+
				               "with parameter: "+eventParameterToStringSerializer(eventParameter)+".\n" +
				               "Caused by: <<unknown cause>>",
				               "threadId",                         to_string(threadId),
				               "listenerMethod["+to_string(i)+"]", to_string((size_t)listenerMethods[i]),
				               "listenerThis["+to_string(i)+"]",   to_string((size_t)listenersThis[i]),
				               "eventParameter",                   eventParameterToStringSerializer(eventParameter));
			}
		}

		// if (zeroCopy && notifyEvents &&  consumeAnswerlessEvents && !consumeAnswerfullEvents)
		void dispatchZeroCopyListeneableAndConsumableAnswerlessEventsLoop(int threadId, void* consumerThis) {
			typename _QueueEventLink::QueueElement* dequeuedEvent;
			unsigned int                            eventId;
			while (isActive) {
				eventId = el.reserveEventForDispatching(dequeuedEvent);
				consumeAnswerlessEvent(threadId, el.answerlessConsumerProcedureReference, consumerThis,     dequeuedEvent->eventParameter);
				notifyEventObservers  (threadId, el.listenerProcedureReferences,          el.listenersThis, dequeuedEvent->eventParameter);
				el.releaseEvent(eventId);
			}
		}

		// if (zeroCopy && notifyEvents && !consumeAnswerlessEvents &&  consumeAnswerfullEvents)
		void dispatchZeroCopyListeneableAndConsumableAnswerfullEventsLoop(int threadId, void* consumerThis) {
			typename _QueueEventLink::QueueElement* dequeuedEvent;
			unsigned int                            eventId;
			while (isActive) {
				eventId = el.reserveEventForDispatching(dequeuedEvent);
				consumeAnswerfullEvent(threadId, el.answerfullConsumerProcedureReference, consumerThis,     dequeuedEvent);
				notifyEventObservers  (threadId, el.listenerProcedureReferences,          el.listenersThis, dequeuedEvent->eventParameter);
				dequeuedEvent->listened = true;
				el.releaseAnswerfullEvent(eventId);
			}
		}

		// if ( zeroCopy && !notifyEvents &&  consumeAnswerlessEvents && !consumeAnswerfullEvents)
		void dispatchZeroCopyConsumableAnswerlessEventsLoop(int threadId, void* consumerThis) {
			typename _QueueEventLink::QueueElement* dequeuedEvent;
			unsigned int                            eventId;
			while (isActive) {
				eventId = el.reserveEventForDispatching(dequeuedEvent);
				consumeAnswerlessEvent(threadId, el.answerlessConsumerProcedureReference, consumerThis, dequeuedEvent->eventParameter);
				el.releaseEvent(eventId);
			}
		}

		// if ( zeroCopy && !notifyEvents && !consumeAnswerlessEvents &&  consumeAnswerfullEvents )
		void dispatchZeroCopyConsumableAnswerfullEventsLoop(int threadId, void* consumerThis) {
			typename _QueueEventLink::QueueElement* dequeuedEvent;
			unsigned int                            eventId;
			while (isActive) {
				eventId = el.reserveEventForDispatching(dequeuedEvent);
				consumeAnswerfullEvent(threadId, el.answerfullConsumerProcedureReference, consumerThis, dequeuedEvent);
				el.releaseEvent(eventId);
			}
		}

		// if ( zeroCopy &&  notifyEvents && !consumeAnswerlessEvents && !consumeAnswerfullEvents )
		void dispatchZeroCopyListeneableEventsLoop(int threadId) {
			typename _QueueEventLink::QueueElement* dequeuedEvent;
			unsigned int                            eventId;
			while (isActive) {
				eventId = el.reserveEventForDispatching(dequeuedEvent);
				notifyEventObservers(threadId, el.listenerProcedureReferences, el.listenersThis, dequeuedEvent->eventParameter);
				el.releaseEvent(eventId);
			}
		}


		void debug() {
			bool isReservationGuardLocked;
			bool isFullGuardNotNull;
			bool isQueueGuardLocked;
			bool isDequeueGuardLocked;
			bool isEmptyGuardNotNull;
			while (isActive) {

				isReservationGuardLocked = !el.reservationGuard.try_lock();
				isFullGuardNotNull       = el.fullGuard != nullptr;
				isQueueGuardLocked       = !el.queueGuard.try_lock();
				isDequeueGuardLocked     = !el.dequeueGuard.try_lock();
				isEmptyGuardNotNull      = el.emptyGuard != nullptr;

				if (!isReservationGuardLocked) el.reservationGuard.unlock();
				if (!isQueueGuardLocked)       el.queueGuard.unlock();
				if (!isDequeueGuardLocked)     el.dequeueGuard.unlock();

				cerr << "\nrHead=" << el.queueReservedHead << "; rTail=" << el.queueReservedTail << "; ((queueReservedTail+1) & 0xFF)=" << ((el.queueReservedTail+1) & 0xFF) << "; reservations[queueReservedHead]=" << el.reservations[el.queueReservedHead] << "; isReservationGuardLocked=" << isReservationGuardLocked << "; isFullGuardNotNull=" << isFullGuardNotNull << "; isQueueGuardLocked=" << isQueueGuardLocked << "; isDequeueGuardLocked=" << isDequeueGuardLocked << "; isEmptyGuardNotNull=" << isEmptyGuardNotNull << endl << flush;
				this_thread::sleep_for(chrono::milliseconds(1000));
			}
		}
	};


}
#endif /* MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_ */
