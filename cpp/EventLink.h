/*
 * IEventLink.h
 *
 *  Created on: Oct 24, 2018
 *      Author: luiz
 */

#ifndef MUTUA_EVENTS_EVENTLINK_H_
#define MUTUA_EVENTS_EVENTLINK_H_

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
	class EventLink {
	public:
		EventLink();
		virtual ~EventLink();
	};

}
#endif /* MUTUA_EVENTS_EVENTLINK_H_ */
