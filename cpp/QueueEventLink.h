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
            _AnswerType*    answerObjectReference;
            mutex           answerMutex;
            bool            reserved;		// keeps track of the conceded but not yet enqueued & conceded but not yet dequeued positions

            QueueElement()
            		: answerObjectReference(nullptr)
            		, reserved(false) {}
        };

        // debug info
        string eventName;

        // consumers
        void (*answerlessConsumerProcedureReference) (void*, const _ArgumentType&);
        void (*answerfullConsumerProcedureReference) (void*, const _ArgumentType&, _AnswerType*, std::mutex&);
        void* answerlessConsumerThis;
        void* answerfullConsumerThis;

        // listeners
        void (*listenerProcedureReferences[_NListeners]) (void*, const _ArgumentType&);
        void* listenersThis[_NListeners];
        unsigned int nListenerProcedureReferences;

        // mutexes
        mutex  reservationGuard;
        alignas(64) mutex* fullGuard;
        mutex  dequeueGuard;
        alignas(64) mutex* emptyGuard;
        mutex  queueGuard;

        // queue
        alignas(64) QueueElement  events[numberOfQueueSlots];	// here are the elements of the queue
        alignas(64) unsigned int  queueHead;          			// will never be behind of 'queueReservedHead'
        alignas(64) unsigned int  queueTail;          			// will never be  ahead of 'queueReservedTail'
        alignas(64) unsigned int  queueReservedHead;  			// will never be  ahead of 'queueHead'
        alignas(64) unsigned int  queueReservedTail;  			// will never be behind of 'queueTail'
        // note: std::hardware_destructive_interference_size seems to not be supported in gcc -- 64 is x86_64 default (possibly the same for armv7)

        QueueEventLink(string eventName)
                : eventName                            (eventName)
                , listenerProcedureReferences          {nullptr}
                , listenersThis                        {nullptr}
                , nListenerProcedureReferences         (0)
                , fullGuard                            (nullptr)
                , emptyGuard                           (nullptr)
                , queueHead                            (0)
                , queueTail                            (0)
                , queueReservedHead                    (0)
                , queueReservedTail                    (0) {

			unsetConsumer();
		}

        template <typename _Class> void setAnswerlessConsumer(void (_Class::*consumerProcedureReference) (const _ArgumentType&), _Class* consumerThis) {
            answerlessConsumerProcedureReference = reinterpret_cast<void (*) (void*, const _ArgumentType&)>(consumerProcedureReference);
            answerlessConsumerThis               = consumerThis;
        }

        template <typename _Class> void setAnswerfullConsumer(void (_Class::*consumerProcedureReference) (const _ArgumentType&, _AnswerType*, std::mutex&), _Class* consumerThis) {
            answerfullConsumerProcedureReference = reinterpret_cast<void (*) (void*, const _ArgumentType&, _AnswerType*, std::mutex&)>(consumerProcedureReference);;
            answerfullConsumerThis               = consumerThis;
        }

        void unsetConsumer() {
        	answerlessConsumerProcedureReference = nullptr;
        	answerfullConsumerProcedureReference = nullptr;
        	answerlessConsumerThis = nullptr;
        	answerfullConsumerThis = nullptr;
        }

        /** Adds a listener to operate on a single instance, regardless of the number of dispatcher threads */
        template <typename _Class> void addListener(void (_Class::*listenerProcedureReference) (const _ArgumentType&), _Class* listenerThis) {
            if (nListenerProcedureReferences >= _NListeners) {
                THROW_EXCEPTION(overflow_error, "Out of listener slots (max="+to_string(_NListeners)+") while attempting to add a new event listener to '" + eventName + "' " +
                                                "(you may wish to increase '_NListeners' at '" + eventName + "'s declaration)");
            }
            listenerProcedureReferences[nListenerProcedureReferences] = reinterpret_cast<void (*) (void*, const _ArgumentType&)>(listenerProcedureReference);
                          listenersThis[nListenerProcedureReferences] = listenerThis;
                  listenersConstructors[nListenerProcedureReferences] = nullptr;
                   listenersDestructors[nListenerProcedureReferences] = nullptr;
            nListenerProcedureReferences++;
        }

        /** Adds a listener to operate on instances created on demand, by calling the _Class' default constructor & destructor */
        template <typename _Class> void addListener(void (_Class::*listenerProcedureReference) (const _ArgumentType&)) {
            if (nListenerProcedureReferences >= _NListeners) {
                THROW_EXCEPTION(overflow_error, "Out of listener slots (max="+to_string(_NListeners)+") while attempting to add a new event listener to '" + eventName + "' " +
                                                "(you may wish to increase '_NListeners' at '" + eventName + "'s declaration)");
            }
            listenerProcedureReferences[nListenerProcedureReferences] = reinterpret_cast<void (*) (void*, const _ArgumentType&)>(listenerProcedureReference);
                          listenersThis[nListenerProcedureReferences] = nullptr;
                  listenersConstructors[nListenerProcedureReferences] = &_Class::ctor;
                   listenersDestructors[nListenerProcedureReferences] = &_Class::dtor;
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
            memcpy(      &(listenersConstructors[pos]),       &(listenersConstructors[pos+1]), (nListenerProcedureReferences - (pos+1)) * sizeof(listenersConstructors[0]));
            memcpy(       &(listenersDestructors[pos]),        &(listenersDestructors[pos+1]), (nListenerProcedureReferences - (pos+1)) * sizeof(listenersDestructors[0]));
            nListenerProcedureReferences--;
            listenerProcedureReferences[nListenerProcedureReferences] = nullptr;
                          listenersThis[nListenerProcedureReferences] = nullptr;
                  listenersConstructors[nListenerProcedureReferences] = nullptr;
                   listenersDestructors[nListenerProcedureReferences] = nullptr;
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

        // CONTINUANDO: só o dispatcher ou o interessado na resposta podem liberar o slot na fila
        // answerless, pode ser liberado pelo dispatcher após todos os listeners terem sido liberados
        // answerfull deve ser liberado pelo dispatcher (se a resposta ja tiver sido obtida) ou pelo
        //            produtor interessado na resposta, se os listeners já tiverem sido executados

        // novidade: answerless ou answerfull QueuedEventLink. Separados. Tambem reentrante e nao reentrante.
        //           ao criar o event link, teremos o número de threads despachando. bool para a reentrancia.
        //           pode ser usado para varificar se o número de threads é permitido pelos nao reentrantes, direct, etc.

        inline _AnswerType* waitForAnswer(unsigned int eventId) {
            QueueElement& event = events[eventId];
            if (event.answerObjectReference == nullptr) {
                THROW_EXCEPTION(runtime_error, "Attempting to wait for an answer from an event of '" + this->eventName + "', which was not prepared to produce an answer. "
                                               "Did you call 'reserveEventForReporting(_ArgumentType)' instead of 'reserveEventForReporting(_ArgumentType&, const _AnswerType&)' ?");
            }
            event.answerMutex.lock();
            event.answerMutex.unlock();
            return event.answerObjectReference;
            // we may now release the event slot if all listeners got notified already
        }

        inline void notifyEventListeners(const _ArgumentType& eventParameter) {
            for (int i=0; i<nListenerProcedureReferences; i++) {
                listenerProcedureReferences[i](listenersThis[i], eventParameter);
            }
        }

        /** Intended to be used by event dispatchers, this method consumes the event using the answerless consumer function pointer.
         *  The queue slot may be immediately released for reused after all listeners get notified (see 'releaseEvent(...)') */
        inline void consumeAnswerlessEvent(QueueElement* event) {
			answerlessConsumerProcedureReference(answerlessConsumerThis, event->eventParameter);
        }

        /** Intended to be used by event dispatchers, this method consumes the event using the answerfull consumer function pointer.
         *  The queue slot may be released for reused (with 'releaseEvent(...)') after:
         *  1) all listeners get notified;
         *  2) the event producer got the 'answer is ready' notification, with 'waitForAnswer(...)' */
        inline void consumeAnswerfullEvent(QueueElement* event) {
			answerfullConsumerProcedureReference(answerfullConsumerThis, event->eventParameter, event->answerObjectReference, event->answerMutex);
                //reentrantlyReleaseSlot(eventId);  must only be released after the producer gets hold of 'answer'
        }

    };
}

#endif /* MUTUA_EVENTS_QUEUEEVENTLINK_H_ */
