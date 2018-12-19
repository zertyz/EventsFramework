#ifndef MUTUA_EVENTS_QUEUEEVENTLINK_H_
#define MUTUA_EVENTS_QUEUEEVENTLINK_H_

#include <iostream>
#include <mutex>
using namespace std;

#include <BetterExceptions.h>
//using namespace mutua::cpputils;


// linux kernel macros for optimizing branch instructions
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

namespace mutua::events {
    /**
     * QueueEventLink.h
     * ================
     * created (in Java) by luiz, Jan 23, 2015, as IEventLink.java
     * transcoded to C++ by luiz, Oct 24, 2018
     * last transcoding  by luiz, Nov  9, 2018
     *
     * Queue based communications between event producers/consumers & notifyers/observers.
     *
    */
    template <typename _AnswerType, typename _ArgumentType, int _NListeners, uint_fast8_t _Log2_QueueSlots>
    class QueueEventLink {

    public:

    	constexpr static unsigned int numberOfQueueSlots = (unsigned int) 1 << (unsigned int) _Log2_QueueSlots;
    	constexpr static unsigned int queueSlotsModulus  = numberOfQueueSlots-1;

        // queue elements
        struct QueueElement {
            _ArgumentType   eventParameter;
            // for answerfull events
            _AnswerType*    answerObjectReference;
            mutex           answerMutex;
            exception_ptr   exception;
            // reserved queue vs completed queue synchronization
            bool            reserved;		// keeps track of the conceded but not yet enqueued & conceded but not yet dequeued slots

            QueueElement()
            		: answerObjectReference(nullptr)
                    , exception(nullptr)
            		, reserved(false) {}
        };

        // consumers
        void       (*answerlessConsumerProcedureReference) (void*, const _ArgumentType&);
        void*       *answerlessConsumerThese;   // pointers to instances (this) of the class on which the method 'answerlessConsumerProcedureReference' will act on
        unsigned int nAnswerlessConsumerThese;
        void       (*answerfullConsumerProcedureReference) (void*, const _ArgumentType&, _AnswerType*, std::mutex&);
        void*       *answerfullConsumerThese;   // pointers to instances (this) of the class on which the method 'answerfullConsumerProcedureReference' will act on
        unsigned int nAnswerfullConsumerThese;

        // listeners
        void       (*listenerProcedureReferences[_NListeners]) (void*, const _ArgumentType&);
        void*        listenersThis[_NListeners];
        unsigned int nListenerProcedureReferences;

        // queue
        alignas(64) QueueElement  events[numberOfQueueSlots];	// here are the elements of the queue
        alignas(64) int  queueHead;          					// will never be behind of 'queueReservedHead'
                    int  queueTail;          					// will never be  ahead of 'queueReservedTail'
                    int  queueReservedHead;  					// will never be  ahead of 'queueHead'
                    int  queueReservedTail;  					// will never be behind of 'queueTail'
        // note: std::hardware_destructive_interference_size seems to not be supported in gcc -- 64 is x86_64 default (possibly the same for armv7)

        // mutexes
        mutex  reservationGuard;
        mutex  dequeueGuard;
        mutex  queueGuard;
        bool   isEmpty;
        bool   isFull;

        // debug info
        string eventName;


        QueueEventLink(string eventName)
                : eventName                            (eventName)
        		, answerlessConsumerProcedureReference (nullptr)
                , answerlessConsumerThese              (nullptr)
				, nAnswerlessConsumerThese             (0)
        		, answerfullConsumerProcedureReference (nullptr)
                , answerfullConsumerThese              (nullptr)
				, nAnswerfullConsumerThese             (0)
                , listenerProcedureReferences          {}
                , listenersThis                        {}
                , nListenerProcedureReferences         (0)
                , isFull                               (false)
                , queueHead                            (0)
                , queueTail                            (0)
                , queueReservedHead                    (0)
                , queueReservedTail                    (0) {

            // queue starts empty & locked for dequeueing
			dequeueGuard.lock();
			isEmpty = true;
		}

        /** Instantiate with an answerless consumer */
        template <typename _Class> QueueEventLink(string eventName, void (_Class::*answerlessConsumerProcedureReference) (const _ArgumentType&), vector<_Class*> thisInstances)
        		: QueueEventLink(eventName) {
        	setAnswerlessConsumer(answerlessConsumerProcedureReference, thisInstances);
        }

        /** Instantiate with an answerfull consumer */
        template <typename _Class> QueueEventLink(void (_Class::*answerfullConsumerProcedureReference) (const _ArgumentType&, _AnswerType*, std::mutex&), vector<_Class*> thisInstances)
        		: QueueEventLink(eventName) {
        	setAnswerfullConsumer(answerfullConsumerProcedureReference, thisInstances);
        }

        /** Destructing a 'QueueEventLink' is only safe when the queues are empty and no threads are waiting to enqueue or dequeue an event */
        ~QueueEventLink() {
        	unsetConsumer();

        	// assure all mutexes are unlocked -- if the statement above is true, this is not needed
        	//for ()
        }

        template <typename _Class> void setAnswerlessConsumer(void (_Class::*consumerProcedureReference) (const _ArgumentType&), vector<_Class*> thisInstances) {
            union {
                void (*genericFuncPtr) (void*, const _ArgumentType&);
                void (_Class::*specificFuncPtr) (const _ArgumentType&);
            };
            // set the generic function pointer from the specific one
            specificFuncPtr                      = consumerProcedureReference;
            answerlessConsumerProcedureReference = genericFuncPtr;
            nAnswerlessConsumerThese             = thisInstances.size();
            // allocate ...
            answerlessConsumerThese              = new void*[nAnswerlessConsumerThese];
            // ... and fill the array
            for (int i=0; i<thisInstances.size(); i++) {
            	answerlessConsumerThese[i] = thisInstances[i];
            }
        }

        template <typename _Class> void setAnswerfullConsumer(void (_Class::*consumerProcedureReference) (const _ArgumentType&, _AnswerType*, std::mutex&), vector<_Class*> thisInstances) {
            union {
                void (*genericFuncPtr) (void*, const _ArgumentType&, _AnswerType*, std::mutex&);
                void (_Class::*specificFuncPtr) (const _ArgumentType&, _AnswerType*, std::mutex&);
            };
            // set the generic function pointer from the specific one
            specificFuncPtr                      = consumerProcedureReference;
            answerfullConsumerProcedureReference = genericFuncPtr;
            nAnswerfullConsumerThese             = thisInstances.size();
            // allocate ...
            answerfullConsumerThese              = new void*[nAnswerfullConsumerThese];
            // ... and fill the array
            for (int i=0; i<thisInstances.size(); i++) {
            	answerfullConsumerThese[i] = thisInstances[i];
            }
        }

        void unsetConsumer() {
        	answerlessConsumerProcedureReference = nullptr;
        	if (answerlessConsumerThese != nullptr) {
        		delete[] answerlessConsumerThese;
        		answerlessConsumerThese  = nullptr;
        	}
            nAnswerlessConsumerThese = 0;
            answerfullConsumerProcedureReference = nullptr;
            if (answerfullConsumerThese != nullptr) {
            	delete[] answerfullConsumerThese;
            	answerfullConsumerThese  = nullptr;
            }
            nAnswerfullConsumerThese = 0;
        }

        /** Adds a listener to operate on a single instance, regardless of the number of dispatcher threads */
        template <typename _Class> void addListener(void (_Class::*listenerProcedureReference) (const _ArgumentType&), _Class* listenerThis) {
            union {
                void (*genericFuncPtr) (void*, const _ArgumentType&);
                void (_Class::*specificFuncPtr) (const _ArgumentType&);
            };
            if (nListenerProcedureReferences >= _NListeners) {
                THROW_EXCEPTION(overflow_error, "Out of listener slots (max="+to_string(_NListeners)+") while attempting to add a new event listener to '" + eventName + "' " +
                                                "(you may wish to increase '_NListeners' at '" + eventName + "'s declaration)");
            }
            // set the generic function pointer from the specific one
            specificFuncPtr                                           = listenerProcedureReference;
            listenerProcedureReferences[nListenerProcedureReferences] = genericFuncPtr;
                          listenersThis[nListenerProcedureReferences] = listenerThis;
            nListenerProcedureReferences++;
        }

        unsigned int findListener(void(&&listenerProcedureReference)(void*, const _ArgumentType&)) {
            for (unsigned int i=0; i<nListenerProcedureReferences; i++) {
                if (listenerProcedureReferences[i] == listenerProcedureReference) {
                    return i;
                }
            }
            return -1;
        }

        bool removeListener(void (&&listenerProcedureReference) (void*, const _ArgumentType&)) {
            unsigned int pos = findListener(listenerProcedureReference);
            if (pos == -1) {
                return false;
            }
            memcpy(&(listenerProcedureReferences[pos]), &(listenerProcedureReferences[pos+1]), (nListenerProcedureReferences - (pos+1)) * sizeof(listenerProcedureReferences[0]));
            memcpy(              &(listenersThis[pos]),               &(listenersThis[pos+1]), (nListenerProcedureReferences - (pos+1)) * sizeof(listenersThis[0]));
            nListenerProcedureReferences--;
            listenerProcedureReferences[nListenerProcedureReferences] = nullptr;
                          listenersThis[nListenerProcedureReferences] = nullptr;
            return true;
        }

        /** Queue length, differently than the queue size, is the number of elements currently waiting to be dequeued */
        int getQueueLength() {
        	scoped_lock<mutex> lock(queueGuard);
        	if (queueTail == queueHead) {
        		if (isFull) {
        			return numberOfQueueSlots;
        		} else {
        			return 0;
        		}
        	}
        	if (queueTail > queueHead) {
        		return queueTail - queueHead;
        	} else {
        		return (numberOfQueueSlots + queueTail) - queueHead;
        	}
        }

        /** Returns the number of slots that are currently holding a dequeuable element + the ones that are currently compromised into holding one of those */
        int getQueueReservedLength() {
        	scoped_lock<mutex> lock(queueGuard);
        	if (queueReservedTail == queueReservedHead) {
        		if (isFull) {
        			return numberOfQueueSlots;
        		} else {
        			return 0;
        		}
        	}
        	if (queueReservedTail > queueReservedHead) {
        		return queueReservedTail - queueReservedHead;
        	} else {
        		return (numberOfQueueSlots + queueReservedTail) - queueReservedHead;
        	}
        }

        /** Common code (without mutexes) for answerless and answerfull events to reserve an 'eventId' slot for further enqueueing -- or,
         *  in other words, enqueueing it on the 'reserved queue'.
         *  In case the queue is full, this method does not block -- it simply returns -1  */
        inline int unguardedReserveEventForReporting(_ArgumentType*& eventParameterPointer) {

        	// is queue full?
			if (unlikely( isFull && (queueReservedTail == queueReservedHead) )) {
				// Queue was already full before the call to this method. Wait on the mutex
				return -1;
			}

			int eventId = queueReservedTail;
			queueReservedTail = (queueReservedTail+1) & queueSlotsModulus;

			// will reserving this element make the queue full? Inform that.
			if (unlikely(queueReservedTail == queueReservedHead)) {
				isFull = true;
			}

			return eventId;
        }

        /** Reserves an 'eventId' (and returns it) for further enqueueing.
         *  Points 'eventParameterPointer' to a location able to be filled with the event information.
         *  This method takes constant time but blocks if the queue is full.
         *  NOTE: the heading of this code should be the same as in the overloaded method. */
        inline int reserveEventForReporting(_ArgumentType*& eventParameterPointer) {

        FULL_QUEUE_RETRY:

			queueGuard.lock();
			int eventId = unguardedReserveEventForReporting(eventParameterPointer);

			if (unlikely(eventId == -1)) {
				// Queue was already full before the call to this method. Wait on the mutex
				queueGuard.unlock();
				reservationGuard.lock();	// 2nd lock. Any caller will wait here. Unlocked only by 'releaseEvent(...)'
				reservationGuard.unlock();
				goto FULL_QUEUE_RETRY;
			}

			// cool! we could reserve a queue slot!

            // reserving a slot for this element made the queue full?
            if (unlikely(isFull)) {
				reservationGuard.lock();		// 1st lock. Won't wait yet.
            }

            // prepare the event slot and return the event id
            QueueElement& futureEvent = events[eventId];
            futureEvent.reserved      = true;
            eventParameterPointer     = &futureEvent.eventParameter;
			queueGuard.unlock();
			return eventId;

        }

        /** Reserves an 'eventId' (and returns it) for further enqueueing.
         *  Points 'eventParameterPointer' to a location able to be filled with the event information.
         *  'answerObjectReference' is a pointer where the 'answerfull' consumer should store the answer -- give a nullptr if the consumer is 'answerless'.
         *  This method takes constant time but blocks if the queue is full.
         *  NOTE: the heading of this code should be the same as in the overloaded method. */
        inline int reserveEventForReporting(_ArgumentType*& eventParameterPointer, _AnswerType* answerObjectReference) {

		FULL_QUEUE_RETRY:

			queueGuard.lock();
			int eventId = unguardedReserveEventForReporting(eventParameterPointer);

			if (unlikely(eventId == -1)) {
				// Queue was already full before the call to this method. Wait on the mutex
				queueGuard.unlock();
				reservationGuard.lock();	// 2nd lock. Any caller will wait here. Unlocked only by 'releaseEvent(...)'
				reservationGuard.unlock();
				goto FULL_QUEUE_RETRY;
			}

			// cool! we could reserve a queue slot!

			// reserving a slot for this element made the queue full?
			if (unlikely(isFull)) {
				reservationGuard.lock();		// 1st lock. Won't wait yet.
			}

			// prepare the event slot and return the event id
            QueueElement& futureEvent         = events[eventId];
            futureEvent.answerObjectReference = answerObjectReference;
            futureEvent.exception             = nullptr;
            futureEvent.reserved              = true;
            eventParameterPointer             = &futureEvent.eventParameter;
			futureEvent.answerMutex.try_lock();		// prepare to wait for the answer
			queueGuard.unlock();
            return eventId;
        }

        /** Signals that the slot at 'eventId' is available for consumption / notification.
         *  This method takes constant time -- a little bit longer if the queue is empty. */
        inline void reportReservedEvent(int eventId) {
            // signal that the slot at 'eventId' is available for dequeueing
        	queueGuard.lock();
        	events[eventId].reserved = false;
            if (likely(eventId == queueTail)) {
            	do {
            		queueTail = (queueTail+1) & queueSlotsModulus;
            	} while (unlikely( (!events[queueTail].reserved) && (queueTail != queueReservedTail) ));
                if (likely(isEmpty)) {
                	isEmpty = false;
                	dequeueGuard.unlock();	// 'emptyGuard' only points to 'dequeueGuard'
                }
            }
			queueGuard.unlock();
        }

        /** Starts the zero-copy dequeueing process.
         *  Points 'dequeuedElementPointer' to the queue location containing the event ready to be consumed & notified, returning the 'eventId'.
         *  This method takes constant time but blocks if the queue is empty. */
        inline int reserveEventForDispatching(QueueElement*& dequeuedElementPointer) {

         EMPTY_QUEUE_RETRY:

			queueGuard.lock();

			// is queue empty?
            if (unlikely( isEmpty && (queueHead == queueTail) )) {
            	// Queue was already empty before the call to this method. Wait on the mutex
            	queueGuard.unlock();
            	dequeueGuard.lock();	// 2nd lock. Any caller will wait here. Unlocked only by 'reportReservedEvent(...)'
            	dequeueGuard.unlock();
            	goto EMPTY_QUEUE_RETRY;
            }

            int eventId = queueHead;
            queueHead = (queueHead+1) & queueSlotsModulus;

            // will dequeueing this element make the queue empty? Set the waiting mutex
            if (queueHead == queueTail) {
            	dequeueGuard.lock();		// 1st lock. Won't wait yet.
            	isEmpty = true;
            }

            dequeuedElementPointer = &events[eventId];
            dequeuedElementPointer->reserved = true;

            queueGuard.unlock();

            return eventId;
        }

        /** Allows 'eventId' reuse (making that slot available for enqueueing a new element) */
        inline void releaseEvent(int eventId) {
        	queueGuard.lock();
        	events[eventId].reserved = false;
            if (likely(eventId == queueReservedHead)) {
            	do {
            		queueReservedHead = (queueReservedHead+1) & queueSlotsModulus;
            	} while (unlikely( (queueReservedHead != queueHead) && (!events[queueReservedHead].reserved) ));
                if (unlikely(isFull)) {
                	isFull = false;
                	reservationGuard.unlock();
                }
            }
			queueGuard.unlock();
        }

        inline _AnswerType* waitForAnswer(int eventId) {
            QueueElement& event                = events[eventId];
            _AnswerType* answerObjectReference = event.answerObjectReference;
            if (answerObjectReference == nullptr) {
                THROW_EXCEPTION(runtime_error, "Attempting to wait for an answer from an event of '" + eventName + "', which was not prepared to produce an answer. "
                                               "Did you call 'reserveEventForReporting(_ArgumentType)' instead of 'reserveEventForReporting(_ArgumentType&, const _AnswerType&)' ?");
            }
            event.answerMutex.lock();	// wait until the answer is ready (the answerfull consumer must unlock the mutex as soon as the answer is ready)
            event.answerMutex.unlock();
            // checks for any exception that might have been thrown
            if (event.exception != nullptr) {
                if (!answerObjectReference) {
                    // exception happened before issuing the answer -- stop the thread flow.
                    std::rethrow_exception(event.exception);
                } else {
                    // exception happened after issuing the answer -- we'll continue with a warning.
                    // note: this code is likely not to be reached if 'waitForAnswer' was called before
                    //       the answer was ready.
                	try {
                        std::rethrow_exception(event.exception);
                	} catch (const exception& e) {
						cerr << "QueueEventLink: exception detected on event '" << eventName <<
								"', after the answerfull consumer successfully produced an answer.\n" <<
								"(note: not all such exceptions will get caught by this method)" <<
								"Caused by: " << e.what() << endl << flush;
                	}
                }
            }
            return answerObjectReference;
        }
    };
}

#undef likely
#undef unlikely

#endif /* MUTUA_EVENTS_QUEUEEVENTLINK_H_ */
