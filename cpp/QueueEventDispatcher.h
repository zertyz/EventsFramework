#ifndef MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_
#define MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_

#include <iostream>
#include <thread>
#include <initializer_list>
using namespace std;

//using namespace mutua::cpputils;


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

		QueueEventDispatcher(_QueueEventLink& el, int nThreads)
				: isActive(true)
				, el(el)
				, nThreads(nThreads+1) {
			threads = new thread[nThreads+1];
			for (int i=0; i<nThreads; i++) {
//cerr << "creating consumer thread #" << i << endl << flush;
				threads[i] = thread(&QueueEventDispatcher::dispatchAnswerlessEventsLoop, this, i);
//this_thread::sleep_for(chrono::milliseconds(300));
			}
			threads[nThreads] = thread(&QueueEventDispatcher::debug, this);
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

		void dispatchAnswerlessEventsLoop(int threadId) {
			typename _QueueEventLink::QueueElement* dequeuedEvent;
			decltype(_QueueEventLink::queueHead)    eventId;
			while (isActive) {
				eventId = el.reserveEventForDispatching(dequeuedEvent);
				if (threadId % 333 == 0) {
					cerr << "thread #" << threadId << " got '" << dequeuedEvent->eventParameter << "'\n" << flush;
				}
				el.consumeAnswerlessEvent(dequeuedEvent);
				el.notifyEventListeners(dequeuedEvent->eventParameter);
				el.releaseEvent(eventId);
			}
		}

		void debug() {
			while (isActive) {
				cerr << "\nrHead=" << el.queueReservedHead << "; rTail=" << el.queueReservedTail << "; ((queueReservedHead+1) & 0xFF)=" << ((el.queueReservedTail+1) & 0xFF) << "; reservations[queueReservedHead]=" << el.reservations[el.queueReservedHead] << endl << flush;
				this_thread::sleep_for(chrono::milliseconds(1000));
			}
		}

		void dispatchAnswerfullEventsLoop() {
//			typename _QueueEventLink::AnswerfullEvent* dequeuedEvent;
//			while (isActive) {
//				el.reserveEventForDispatching(dequeuedEvent);
//				el.consumeAnswerfullEvent(dequeuedEvent);
//				el.notifyEventListeners(dequeuedEvent->eventParameter);
//				el.releaseEvent(dequeuedEvent);
//			}
		}

	};


}
#endif /* MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_ */
