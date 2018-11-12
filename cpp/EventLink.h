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
        setAnswerlessConsumer()
        setAnswerfullConsumer()
        unsetConsumer()
        addListener()
        removeListener()

        reserveEventForReporting()
        reportReservedEvent()
        reserveEventForDispatching()

        waitForAnswer()
        notifyEventListeners()
		consumeAnswerlessEvent()
		consumeAnswerfullEvent()

        releaseEvent()
*/
	};

}
#endif /* MUTUA_EVENTS_EVENTLINK_H_ */
