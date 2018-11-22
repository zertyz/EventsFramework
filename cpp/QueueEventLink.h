#ifndef MUTUA_EVENTS_QUEUEEVENTLINK_H_
#define MUTUA_EVENTS_QUEUEEVENTLINK_H_

#include <iostream>
#include <mutex>
using namespace std;

#include <BetterExceptions.h>
using namespace mutua::cpputils;


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
        struct alignas(64) QueueElement {
            _ArgumentType   eventParameter;
            // for answerfull events
            _AnswerType*    answerObjectReference;
            mutex           answerMutex;
            exception_ptr   exception;
            bool            answered;
            bool            listened;
            // queue synchronization
            bool            reserved;		// keeps track of the conceded but not yet enqueued & conceded but not yet dequeued positions

            QueueElement()
            		: answerObjectReference(nullptr)
                    , exception(nullptr)
                    , answered(false)
                    , listened(false)
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

        // mutexes
        alignas(64) mutex  reservationGuard;
        alignas(64) mutex* fullGuard;
        alignas(64) mutex  dequeueGuard;
        alignas(64) mutex* emptyGuard;
        alignas(64) mutex  queueGuard;

        // queue
        alignas(64) QueueElement  events[numberOfQueueSlots];	// here are the elements of the queue
        alignas(64) unsigned int  queueHead;          			// will never be behind of 'queueReservedHead'
        alignas(64) unsigned int  queueTail;          			// will never be  ahead of 'queueReservedTail'
        alignas(64) unsigned int  queueReservedHead;  			// will never be  ahead of 'queueHead'
        alignas(64) unsigned int  queueReservedTail;  			// will never be behind of 'queueTail'
        // note: std::hardware_destructive_interference_size seems to not be supported in gcc -- 64 is x86_64 default (possibly the same for armv7)

        // debug info
        string eventName;


        QueueEventLink(string eventName)
                : eventName                            (eventName)
                , answerlessConsumerThese              (nullptr)
				, nAnswerlessConsumerThese             (0)
                , answerfullConsumerThese              (nullptr)
				, nAnswerfullConsumerThese             (0)
                , listenerProcedureReferences          {nullptr}
                , listenersThis                        {nullptr}
                , nListenerProcedureReferences         (0)
                , fullGuard                            (nullptr)
                , emptyGuard                           (nullptr)
                , queueHead                            (0)
                , queueTail                            (0)
                , queueReservedHead                    (0)
                , queueReservedTail                    (0) {}

        ~QueueEventLink() {
        	unsetConsumer();
        }

        template <typename _Class> void setAnswerlessConsumer(void (_Class::*consumerProcedureReference) (const _ArgumentType&), vector<_Class*> thisInstances) {
            answerlessConsumerProcedureReference = reinterpret_cast<void (*) (void*, const _ArgumentType&)>(consumerProcedureReference);
            nAnswerlessConsumerThese             = thisInstances.size();
            // allocate ...
            answerlessConsumerThese              = new void*[nAnswerlessConsumerThese];
            // ... and fill the array
            int i=0;
            for (_Class* instance : thisInstances) {
            	answerlessConsumerThese[i++] = instance;
            }
        }

        template <typename _Class> void setAnswerfullConsumer(void (_Class::*consumerProcedureReference) (const _ArgumentType&, _AnswerType*, std::mutex&), vector<_Class*> thisInstances) {
            answerfullConsumerProcedureReference = reinterpret_cast<void (*) (void*, const _ArgumentType&, _AnswerType*, std::mutex&)>(consumerProcedureReference);;
            nAnswerfullConsumerThese             = thisInstances.size();
            // allocate ...
            answerfullConsumerThese              = new void*[nAnswerfullConsumerThese];
            // ... and fill the array
            int i=0;
            for (_Class* instance : thisInstances) {
            	answerfullConsumerThese[i++] = instance;
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
            if (nListenerProcedureReferences >= _NListeners) {
                THROW_EXCEPTION(overflow_error, "Out of listener slots (max="+to_string(_NListeners)+") while attempting to add a new event listener to '" + eventName + "' " +
                                                "(you may wish to increase '_NListeners' at '" + eventName + "'s declaration)");
            }
            listenerProcedureReferences[nListenerProcedureReferences] = reinterpret_cast<void (*) (void*, const _ArgumentType&)>(listenerProcedureReference);
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

        /** Reserves an 'eventId' (and returns it) for further enqueueing.
         *  Points 'eventParameterPointer' to a location able to be filled with the event information.
         *  This method takes constant time but blocks if the queue is full. */
        inline unsigned int reserveEventForReporting(_ArgumentType*& eventParameterPointer) {
		FULL_QUEUE_RETRY:
			// reserve a queue slot
			queueGuard.lock();
			if (((queueReservedTail+1) & queueSlotsModulus) == queueReservedHead) {
				if ( events[queueReservedHead].reserved || (queueReservedHead == queueHead) ) {
					// queue is full. Wait
					if (fullGuard == nullptr) {
						reservationGuard.lock();
						fullGuard = &reservationGuard;
					}
					queueGuard.unlock();
					reservationGuard.lock();
					reservationGuard.unlock();
					goto FULL_QUEUE_RETRY;
				} else {
					queueReservedHead = (queueReservedHead+1) & queueSlotsModulus;
				}
			}
			unsigned int eventId = queueReservedTail;
			queueReservedTail = (queueReservedTail+1) & queueSlotsModulus;

			// prepare the event slot and return the event id
            QueueElement& futureEvent         = events[eventId];
			eventParameterPointer             = &futureEvent.eventParameter;
            futureEvent.reserved              = true;

			queueGuard.unlock();

			return eventId;
        }

        /** Reserves an 'eventId' (and returns it) for further enqueueing.
         *  Points 'eventParameterPointer' to a location able to be filled with the event information.
         *  'answerObjectReference' is a pointer where the 'answerfull' consumer should store the answer -- give a nullptr if the consumer is 'answerless'.
         *  This method takes constant time but blocks if the queue is full. */
        inline unsigned int reserveEventForReporting(_ArgumentType*& eventParameterPointer, _AnswerType* answerObjectReference) {
        	unsigned int eventId = reserveEventForReporting(eventParameterPointer);
            QueueElement& futureEvent         = events[eventId];
            futureEvent.answerObjectReference = answerObjectReference;
            futureEvent.answered              = false;
            futureEvent.listened              = false;
            futureEvent.exception             = nullptr;
			futureEvent.answerMutex.try_lock();		// prepare to wait for the answer
            return eventId;
        }

        /** Signals that the slot at 'eventId' is available for consumption / notification.
         *  This method takes constant time -- a little bit longer if the queue is empty. */
        inline void reportReservedEvent(unsigned int eventId) {
            // signal that the slot at 'eventId' is available for dequeueing
        	queueGuard.lock();
        	events[eventId].reserved = false;
            if (eventId == queueTail) {
                queueTail = (queueTail+1) & queueSlotsModulus;
                // unlock if someone was waiting on the empty queue
                queueGuard.unlock();
                if (emptyGuard) {
                	emptyGuard->unlock();
                	emptyGuard = nullptr;
                }
            } else {
            	queueGuard.unlock();
            }
        }

        /** Starts the zero-copy dequeueing process.
         *  Points 'dequeuedElementPointer' to the queue location containing the event ready to be consumed & notified, returning the 'eventId'.
         *  This method takes constant time but blocks if the queue is empty. */
        inline unsigned int reserveEventForDispatching(QueueElement*& dequeuedElementPointer) {
         EMPTY_QUEUE_RETRY:
            // dequeue but don't release the slot yet
			queueGuard.lock();
            if (queueHead == queueTail) {
                if ( events[queueTail].reserved || (queueTail == queueReservedTail) ) {
                    // queue is empty -- wait until 'reentrantlyReportReservedEvent(...)' unlocks 'emptyGuard'
					if (emptyGuard == nullptr) {
						dequeueGuard.lock();
						emptyGuard = &dequeueGuard;
					}
                	queueGuard.unlock();
                    dequeueGuard.lock();
                    dequeueGuard.unlock();
                    goto EMPTY_QUEUE_RETRY;
                } else {
                    queueTail = (queueTail+1) & queueSlotsModulus;
                }
            }
            unsigned int eventId = queueHead;
            queueHead = (queueHead+1) & queueSlotsModulus;

            dequeuedElementPointer           = &events[eventId];
            dequeuedElementPointer->reserved = true;

            queueGuard.unlock();

            return eventId;
        }

        /** Allow 'eventId' reuse (make that slot available for enqueueing a new element).
          * Answerless events call it uppon consumption & notifications;
          * Answerfull events call it after notifications and after the event producer gets hold of the 'answer' object reference.
          * This method takes constant time -- a little longer when the queue is full. */
        inline void releaseEvent(unsigned int eventId) {
            // signal that the slot at 'eventId' is available for enqueue reservations
        	queueGuard.lock();
        	events[eventId].reserved = false;
            if (eventId == queueReservedHead) {
                queueReservedHead = (queueReservedHead+1) & queueSlotsModulus;
                // unlock if someone was waiting on the full queue
                queueGuard.unlock();
                if (fullGuard) {
                	fullGuard->unlock();
                	fullGuard = nullptr;
                }
            } else {
            	queueGuard.unlock();
            }
        }

        /** This method should be called both after notifications got processed and after 'waitForAnswer' done its job.
          * Alloes 'eventId' to be reused. */
        inline bool releaseAnswerfullEvent(unsigned int eventId) {
            if (events[eventId].answered && events[eventId].listened) {
                releaseEvent(eventId);
                return true;
            } else {
                return false;
            }
        }

        // CONTINUANDO: só o dispatcher ou o interessado na resposta podem liberar o slot na fila
        // answerless, pode ser liberado pelo dispatcher após todos os listeners terem sido liberados
        // answerfull deve ser liberado pelo dispatcher (se a resposta ja tiver sido obtida) ou pelo
        //            produtor interessado na resposta, se os listeners já tiverem sido executados

        // novidade: answerless ou answerfull QueuedEventLink. Separados. Tambem reentrante e nao reentrante.
        //           ao criar o event link, teremos o número de threads despachando. bool para a reentrancia.
        //           pode ser usado para varificar se o número de threads é permitido pelos nao reentrantes, direct, etc.

        inline _AnswerType* waitForAnswer(unsigned int eventId) {
            QueueElement& event = events[eventId];
            _AnswerType*    answerObjectReference = event->answerObjectReference;
            if (answerObjectReference == nullptr) {
                THROW_EXCEPTION(runtime_error, "Attempting to wait for an answer from an event of '" + eventName + "', which was not prepared to produce an answer. "
                                               "Did you call 'reserveEventForReporting(_ArgumentType)' instead of 'reserveEventForReporting(_ArgumentType&, const _AnswerType&)' ?");
            }
            // wait until the answer is ready (the answerfull consumer must unlock the mutex as soon as the answer is ready)
            event.answerMutex.lock();
            event.answerMutex.unlock();
            event.answered = true;
            // checks for any exception that might have been thrown
            if (event.exception != nullptr) {
                if (!answerObjectReference) {
                    // exception happened before issueing the answer -- stop the thread flow.
                    std::rethrow_exception(event.exception);
                } else {
                    // exception happened after issueing the answer -- we'll continue with a warning.
                    // note: this code is likely not to be reached if 'waitForAnswer' was called before
                    //       the answer was ready.
                    cerr << "QueueEventLink: exception detected on event '" << eventName <<
                            "', after the answerfull consumer sucessfully procuced an answer.\n" <<
                            "(note: not all such exceptions will get caught by this method)" <<
                            "Caused by: " << event.exception.what() << endl << flush;
                }
            }
            releaseAnswerfullEvent(eventId);
            return answerObjectReference;
        }
    };
}

#endif /* MUTUA_EVENTS_QUEUEEVENTLINK_H_ */
