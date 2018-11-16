#ifndef MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_
#define MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_

#include <iostream>
#include <thread>
#include <initializer_list>
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

		bool              isActive;
		_QueueEventLink& el;
		int               nThreads;
		thread*           threads;

		QueueEventDispatcher(_QueueEventLink& el,
		                     int              nThreads,
		                     int              threadsPriority,
		                     // TODO consumers 'this' instances and
		                     // TODO listeners 'this' instances should be set on the event link?
		                     // TODO or they must receive a generator of new instances?
							 bool             zeroCopy,
							 bool             notifyEvents,
							 bool             consumeAnswerlessEvents,
							 bool             consumeAnswerfullEvents)
				: isActive(true)
				, el(el)
				, nThreads(nThreads/*+1*/) {

			if (threadsPriority != 0) {
                THROW_EXCEPTION(not_implemented_error, "Attempting to create a dispatcher for event '"+el.eventName+"' with custom 'threadsPriority', " +
                                                       "and this is not implemented yet -- it must be zero in the meantime.");
			}

			threads = new thread[nThreads/*+1*/];

			for (int i=0; i<nThreads; i++) {
				/**/ if (zeroCopy && notifyEvents &&  consumeAnswerlessEvents && !consumeAnswerfullEvents)
					threads[i] = thread(&QueueEventDispatcher::dispatchZeroCopyListeneableAndConsumableAnswerlessEventsLoop, this, i);
				else if (zeroCopy && notifyEvents && !consumeAnswerlessEvents &&  consumeAnswerfullEvents)
					threads[i] = thread(&QueueEventDispatcher::dispatchZeroCopyListeneableAndConsumableAnswerlessEventsLoop, this, i);
				else
	                THROW_EXCEPTION(not_implemented_error, "Attempting to create a dispatcher for event '"+el.eventName+"' with a not implemented combination of " +
	                                                       "'zeroCopy' (" +                to_string(zeroCopy)+"), " +
	                                                       "'notifyEvents' (" +            to_string(notifyEvents)+"), " +
	                                                       "'consumeAnswerlessEvents' (" + to_string(consumeAnswerlessEvents)+") and " +
	                                                       "'consumeAnswerfullEvents' (" + to_string(consumeAnswerfullEvents)+")");
			}
			/*threads[nThreads] = thread(&QueueEventDispatcher::debug, this);*/
		}

		~QueueEventDispatcher() {
			stopASAP();
			delete[] threads;
		}

		void stopASAP() {
			std::cerr << "Tentando parar..." << std::endl << std::flush;
			if (isActive) {
				for (int i=0; i<nThreads; i++) {
					std::cerr << "detaching thread #" << i << std::endl << std::flush;
					threads[i].detach();
				}
				isActive = false;
			}
		}

		// if (zeroCopy && notifyEvents &&  consumeAnswerlessEvents && !consumeAnswerfullEvents)
		void dispatchZeroCopyListeneableAndConsumableAnswerlessEventsLoop(int threadId) {
			typename _QueueEventLink::QueueElement* dequeuedEvent;
			unsigned int                            eventId;
			while (isActive) {
				eventId = el.reserveEventForDispatching(dequeuedEvent);
				el.consumeAnswerlessEvent(dequeuedEvent);
				el.notifyEventListeners(dequeuedEvent->eventParameter);
				el.releaseEvent(eventId);
			}
		}

		// if (zeroCopy && notifyEvents && !consumeAnswerlessEvents &&  consumeAnswerfullEvents)
		void dispatchZeroCopyListeneableAndConsumableAnswerfullEventsLoop() {
			typename _QueueEventLink::AnswerfullEvent* dequeuedEvent;
			while (isActive) {
				el.reserveEventForDispatching(dequeuedEvent);
				el.consumeAnswerfullEvent(dequeuedEvent);
				el.notifyEventListeners(dequeuedEvent->eventParameter);
				// TODO consultar regras para liberar evento com resposta
				// el.releaseEvent(dequeuedEvent);
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
