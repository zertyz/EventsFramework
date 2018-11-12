#ifndef MUTUA_EVENTS_QUEUEDCLASSEVENTLINK_H_
#define MUTUA_EVENTS_QUEUEDCLASSEVENTLINK_H_

#include <mutex>
using namespace std;

#include <BetterExceptions.h>
using namespace mutua::cpputils;


namespace mutua::events {
    /**
     * QueuedClassEventLink.h
     * ======================
     * created (in Java) by luiz, Jan 23, 2015, as IEventLink.java
     * transcoded to C++ by luiz, Oct 24, 2018
     * last transcoding  by luiz, Nov  9, 2018
     *
     * Queue based communications between event producers/consumers & notifyers/observers.
     *
    */
	
    template <typename _AnswerType, typename _ArgumentType, int _NListeners, typename _QueueSlotsType>
    class QueuedClassEventLink {

        // answers
        struct QueueElement {
            _AnswerType*    answerObjectReference;
            _ArgumentType   eventParameter;
            mutex           answerMutex;

            QueueElement()
            		: answerObjectReference(nullptr) {}
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
        int nListenerProcedureReferences;

        // mutexes
        mutex  reservationGuard;
        mutex* fullGuard;
        mutex  enqueueGuard;
        mutex  dequeueGuard;
        mutex* emptyGuard;

        // queue
        QueueElement    events      [(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];  // an array sized like this allows implicit modulus operations on indexes of the same type (_QueueSlotsType)
        bool            reservations[(size_t)std::numeric_limits<_QueueSlotsType>::max()+(size_t)1];  // keeps track of the conceded but not yet enqueued & conceded but not yet dequeued positions
        _QueueSlotsType queueHead;          // will never be behind of 'queueReservedHead'
        _QueueSlotsType queueTail;          // will never be  ahead of 'queueReservedTail'
        _QueueSlotsType queueReservedHead;  // will never be  ahead of 'queueHead'
        _QueueSlotsType queueReservedTail;  // will never be behind of 'queueTail'


        QueuedClassEventLink(string eventName)
                : eventName                            (eventName)
                , listenerProcedureReferences          {nullptr}
                , listenersThis                        {nullptr}
                , nListenerProcedureReferences         (0)
                , fullGuard                            (nullptr)
                , emptyGuard                           (nullptr)
                , reservations                         {false}
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

        template <typename _Class> void addListener(void (_Class::*listenerProcedureReference) (const _ArgumentType&), _Class* listenerThis) {
            if (nListenerProcedureReferences >= _NListeners) {
                THROW_EXCEPTION(overflow_error, "Out of listener slots (max="+to_string(_NListeners)+") while attempting to add a new event listener to '" + eventName + "' " +
                                                "(you may wish to increase '_NListeners' at '" + eventName + "'s declaration)");
            }
            listenerProcedureReferences[nListenerProcedureReferences] = reinterpret_cast<void (*) (void*, const _ArgumentType&)>(listenerProcedureReference);
                          listenersThis[nListenerProcedureReferences] = listenerThis;
            nListenerProcedureReferences++;
        }

        int findListener(void(&&listenerProcedureReference)(void*, const _ArgumentType&)) {
            for (int i=0; i<nListenerProcedureReferences; i++) {
                if (listenerProcedureReferences[i] == listenerProcedureReference) {
                    return i;
                }
            }
            return -1;
        }

        bool removeListener(void (&&listenerProcedureReference) (void*, const _ArgumentType&)) {
            int pos = findListener(listenerProcedureReference);
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
         *  'answerObjectReference' is a pointer where the 'answerfull' consumer should store the answer -- give a nullptr if the consumer is 'answerless'.
         *  This method takes constant time but blocks if the queue is full. */
        inline int reserveEventForReporting(_ArgumentType*& eventParameterPointer, _AnswerType* answerObjectReference) {

        FULL_QUEUE_RETRY:
            // reserve a queue slot
            reservationGuard.lock();
            if (queueReservedHead-queueReservedTail == 1) {
                if (reservations[queueReservedHead]) {
                    // queue is full. Wait
                    fullGuard = &reservationGuard;
                    goto FULL_QUEUE_RETRY;
                    //reservationGuard.lock();
                } else {
                    queueReservedHead++;
                }
            }
            _QueueSlotsType eventId = queueReservedTail++;
            reservationGuard.unlock();

            // prepare the event slot and return the event id
            reservations[eventId]              = true;
            QueueElement& futureEvent          = events[eventId];
            eventParameterPointer              = &futureEvent.eventParameter;
            futureEvent.answerObjectReference  = answerObjectReference;
            // prepare to wait for the answer
            if (answerObjectReference != nullptr) {
                futureEvent.answerMutex.try_lock();
            }
            return eventId;
        }

        /** Reserves an 'eventId' (and returns it) for further enqueueing.
         *  Points 'eventParameterPointer' to a location able to be filled with the event information.
         *  This method takes constant time but blocks if the queue is full. */
        inline int reserveEventForReporting(_ArgumentType*& eventParameter) {
            return reserveEventForReporting(eventParameter, nullptr);
        }

        /** Signals that the slot at 'eventId' is available for consumption / notification.
         *  This method takes constant time -- a little bit longer if the queue is empty. */
        inline void reportReservedEvent(_QueueSlotsType eventId) {
            // signal that the slot at 'eventId' is available for dequeueing
            reservations[eventId] = false;
            if (eventId == queueTail) {
                queueTail++;
                // unlock if someone was waiting on the empty queue
                if (emptyGuard) {
                    mutex* guard = emptyGuard;
                    emptyGuard = nullptr;
                    guard->unlock();
                }
            }
        }

        /** Starts the zero-copy dequeueing process.
         *  Points 'dequeuedElementPointer' to the queue location containing the event ready to be consumed & notified, returning the 'eventId'.
         *  This method takes constant time but blocks if the queue is empty. */
        inline _QueueSlotsType reserveEventForDispatching(QueueElement*& dequeuedElementPointer) {
            
        EMPTY_QUEUE_RETRY:
            // dequeue but don't release the slot yet
            dequeueGuard.lock();
            if (queueHead == queueTail) {
                if (reservations[queueTail]) {
                    // queue is empty -- wait until 'reentrantlyReportReservedEvent(...)' unlocks 'emptyGuard'
                    emptyGuard = &dequeueGuard;
                    goto EMPTY_QUEUE_RETRY;
                } else {
                    queueTail++;
                }
            }
            _QueueSlotsType eventId = queueHead++;
            dequeueGuard.unlock();

            reservations[eventId] = true;
            dequeuedElementPointer = &events[eventId];
            return eventId;
        }

        /** Allow 'eventId' reuse (make that slot available for enqueueing a new element).
          * Answerless events call it uppon consumption & notifications;
          * Answerfull events call it after notifications and after the event producer gets hold of the 'answer' object reference.
          * This method takes constant time -- a little longer when the queue is full. */
        inline void releaseEvent(_QueueSlotsType eventId) {
            // signal that the slot at 'eventId' is available for enqueue reservations
            reservations[eventId] = false;
            if (eventId == queueReservedHead) {
                queueReservedHead++;
                // unlock if someone was waiting on the full queue
                if (fullGuard) {
                    mutex* guard = fullGuard;
                    fullGuard = nullptr;
                    guard->unlock();
                }
            }
        }

        // CONTINUANDO: só o dispatcher ou o interessado na resposta podem liberar o slot na fila
        // answerless, pode ser liberado pelo dispatcher após todos os listeners terem sido liberados
        // answerfull deve ser liberado pelo dispatcher (se a resposta ja tiver sido obtida) ou pelo
        //            produtor interessado na resposta, se os listeners já tiverem sido executados

        // novidade: answerless ou answerfull QueuedEventLink. Separados. Tambem reentrante e nao reentrante.
        //           ao criar o event link, teremos o número de threads despachando. bool para a reentrancia.
        //           pode ser usado para varificar se o número de threads é permitido pelos nao reentrantes, direct, etc.

        inline _AnswerType* waitForAnswer(_QueueSlotsType eventId) {
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
        inline void consumeAnswerlessEvent(QueueElement* event, _QueueSlotsType eventId) {
			answerlessConsumerProcedureReference(answerlessConsumerThis, event->eventParameter);
        }

        /** Intended to be used by event dispatchers, this method consumes the event using the answerfull consumer function pointer.
         *  The queue slot may be released for reused (with 'releaseEvent(...)') after:
         *  1) all listeners get notified;
         *  2) the event producer got the 'answer is ready' notification, with 'waitForAnswer(...)' */
        inline void consumeAnswerfullEvent(QueueElement* event, _QueueSlotsType eventId) {
			answerfullConsumerProcedureReference(answerfullConsumerThis, event->eventParameter, event->answerObjectReference, event->answerMutex);
                //reentrantlyReleaseSlot(eventId);  must only be released after the producer gets hold of 'answer'
        }

    };
}
#endif /* MUTUA_EVENTS_QUEUEDCLASSEVENTLINK_H_ */
