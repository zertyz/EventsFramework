# EventsFramework
A framework for efficient (local or distributed) event driven programming, easing the creation of decoupled architectures and respecting real-time constraints.


Introduction
============

Event driven architectures aims to increase development productivity by allowing further decoupling of components. The core concepts of Event Driven Programming are at the center of some serious and currently fashioned technologies:

  - Callbacks and reactive programming
  - Parallel computing
  - Distributed execution
  - Plugin architectures
  - Declarative Programming (related to the program's control flow, in oposition to Imperative Programming)

This framework provides mechanisms for efficient producing and consuming (and also notifying and observing) events. Events can be "consumed" or "observed". For instance, when a server receives a request it needs to provide a response -- this is an example of an event that was produced and consumed. On the other hand, when you plug a USB Stick on your computer, there might be a software to show the files: if there is, it will show; if there is not, nothing will happen. This is an example of an event who got notifyed but for which there may or may not be observers listening -- the computer will keep working fine if no one is listening for such events.

Consumable Events may also be synchronous or asynchronous. Asynchronous events are the ones which you will not handle the answer. Synchronous, therefore, are the ones which you want the answer in order to proceed on your algorithm.

  - An example of a "Synchronous Consumable Event" is the server response given above: the same socket that started the request must wait for the answer -- and the server thread must produce a consumable event, dispatch it to be processed elsewhere and wait until there is an answer to be written back to the socket.
  - An "Asynchronous Consumable Event" example might be a notification event, lets say, for the system operators in case some error was detected: the server produces the event and resumes it's business. The "Event Consumer for the Server's Notification Events" might, on another thread, choose to send an email, sms, mobile push notification or just log it. The event producer (the Server Internal Loop) don't need any answer from such events -- and these events should never be ignored as well: even if there are no consumers at the moment a Consumable Event is generated, this framework will keep track of it until one gets registered.

Now, back to Listenable Events: they have some similarities to Consumable Events, but:

  - Listenable Events may have zero or more listeners, while Consumable Events should have exactly one consumer -- here we are refering to "listener methods" and "consumer methods", not threads.
  - Listenable Events might be ignored -- if there are no listeners and events are notifyed, they are discarded. Conversely, when there are no consumers registered for a Consumable Event, they are enqueued or an error is thrown.
  - Listenable Events are not Synchronous nor Asynchronous for we don't care for any answer a listener might give us -- One can think of Listenable Events to be similar to "Asynchronous Consumable Events" in this regard.

Sometimes there are cases in which it is difficult, by the definitions given above, to determine if we must consider an event a consumable or a listenable event:

  - Imagine the server's mechanism for incrementing the usage metrics of their services: the event is generated with the needed information, but the main server code don't care if it will be listened by "Log Writters", "Metrics Gathering" or some sort of "DDoS Pattern Detection" algorithm. This should be considered a Listenable Event, since an instance of such server might work without these features.


The case for "Synchronous Consumable Event" vs method invocation
================================================================

When one looks at the simplifyed execution flow of a Synchronous Consumable Event, one might see:

	registerEventConsumer(MY_EVENT_TYPE, MY_METHOD)  // consumer
	(...)
	myEvent  = produceEvent(MY_EVENT_TYPE, params)   // producer
	reportConsumableEvent(myEvent)
	(...)
	answer = waitForAnswer(myEvent)                  // will wait for execution of MY_METHOD(params)

Which is remakably similar to a standard function call:

    answer = MY_METHOD(params)

So, before we dismiss the Synchronous Consumable Events, lets look at some features of this framework:

    1) Distributed execution -- METHOD might be executed on another machine;
    2) Interchanging the control flow without affecting the program logic -- METHOD might be executed on another machine or thread, for efficiency reasons, or on the same thread, for easily testing and debugging the logic;
    3) Resource Optimization and Overloading Protection -- considering simultaneous MY_METHOD executions, there is likely to be a number which will produce the maximum executions per minute a machine can handle. Numbers above it will put the machine into an overloading state and numbers below it will not use all it's capacity. Regardless of the number of events produced, only n threads must be executing MY_METHODs;
    4) Processing Latency lowering -- parallel execution of 'MY_METHOD' may start as soon as you have 'params'. When you're ready to get the 'answer', it will likely take less time;
    5) Fault tolerancy and operational flexibility -- Events might get their execution delayed or even recorded on a fallback queue, but are never ignored.

We consider any of the reasons above to be enough to justify the use case.


- https://nikitablack.github.io/2016/04/12/Generic-C-delegates.html
- https://www.codeproject.com/Articles/1170503/The-Impossibly-Fast-Cplusplus-Delegates-Fixed

Real-time requisites
====================

