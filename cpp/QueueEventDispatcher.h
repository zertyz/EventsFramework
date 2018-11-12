#ifndef MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_
#define MUTUA_EVENTS_QUEUEEVENTDISPATCHER_H_

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
				, nThreads(nThreads) {
			threads = new thread[nThreads];
			for (int i=0; i<nThreads; i++) {
//cerr << "creating consumer thread #" << i << endl << flush;
				threads[i] = thread(&QueueEventDispatcher::dispatchAnswerlessEventsLoop, this);
//this_thread::sleep_for(chrono::milliseconds(300));
			}
		}

		~QueueEventDispatcher() {
			stopASAP();
			delete[] threads;
		}

		void stopASAP() {
			if (isActive) {
				for (int i=0; i<nThreads; i++) {
					threads[i].detach();
				}
				isActive = false;
			}
		}

		void dispatchAnswerlessEventsLoop() {
			typename _QueueEventLink::AnswerfullEvent* dequeuedEvent;
			while (isActive) {
				el.reserveEventForDispatching(dequeuedEvent);
				el.consumeAnswerlessEvent(dequeuedEvent);
				el.notifyEventListeners(dequeuedEvent->eventParameter);
				el.releaseEvent(dequeuedEvent);
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
