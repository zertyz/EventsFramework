#ifndef MUTUA_EVENTS_EVENTLINK_H_
#define MUTUA_EVENTS_EVENTLINK_H_

#include <string>
using namespace std;


namespace mutua::events {
    /**
     * EventLink.h
     * ===========
     * created (in Java) by luiz, Jan 23, 2015, as IEventLink.java
     * transcoded to C++ by luiz, Oct 24, 2018
     * last transcoding  by luiz, Nov  9, 2018
     *
     * This is the base of Mutua's event driven architecture.
     *
     * Defines how event producers/consumers & notifyers/observers will communicate.
     *
    */
	template <class _ChildEventLink>
    class EventLink {
    
    private:
        _ChildEventLink& instance;

	public:

        EventLink(_ChildEventLink& instance)
                : instance(instance) {}

        // debug info
        inline string getEventName()                 { return instance.eventName; }
        inline int    getNumberOfDispatcherThreads() { return instance.numberOfDispatcherThreads; }
        inline bool   allowReentrantEventReporting() { return instance.allowReentrantEventReporting; }
        inline bool   allowReentrantDispatching()    { return instance.allowReentrantDispatching; }
        inline bool   isAnswerfull()                 { return instance.isAnswerfull; }
        inline bool   isZeroCopy()                   { return instance.isZeroCopy; }

        // api
        /*
        setAnswerlessConsumer(idem abaixo)
        setAnswerfullConsumer(idem abaixo)
        unsetConsumer() -- chama o destructorLambda, se foi fornecido
        addListener(methodPtr, instantiatorLambda, destructorLambda)
        addListener(methodPtr, methodPtrThis)
        removeListener() -- chama o destructor lambda se foi fornecido

        // zero copy queue API
        reserveEventForReporting()
        reportReservedEvent()
        reserveEventForDispatching()

        waitForAnswer()
        notifyEventListeners()
		consumeAnswerlessEvent()
		consumeAnswerfullEvent()

        // zero-copy queue API
        releaseEvent()

        // non-zero-copy queue API
        enqueueEvent()  -- consulta o head e move o tail (mutex somente ao bloquear, 1 única thread enfileirando)
        dequeueEvent()  -- consulta o tail e move o heaf (mutex somente ao bloquear, 1 única thread desenfileirando, que pode não ser a mesma que enfilera)

        // object pool instantiation/deinstantiation
        constructMethodObject(constructorLambda) -- executa, obtendo a instância no retorno -- o argumento é o número da instância
        destructMethodObject(destructorLambda)   -- executa, passando a instancia como argumento

        estes métodos são chamados pelo 'EventDispatcher' e/ou removeListener / unsetConsumer -- se é que unsetConsumer faz sentido.
*/
	};

}
#endif /* MUTUA_EVENTS_EVENTLINK_H_ */
