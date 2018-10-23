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

Consumable Events may also be synchronous or asynchronous; and require a functional or procedural consumer. Asynchronous events are the ones which you don't care when they are consumed and synchronous, the opposite. Functional consumers will produce an answer and procedural consumers will not. The folowing table shows the possible combinations:

	case #  synchronicity	     consumer sub-routine    valid?
	  1         sync                   procedure          yes      
	  2         async                  procedure          yes
	  3         sync                   function           yes
	  4         async                  function           no

These classifications are important for they have performance implications. Case 2 is the most performant. Case 1 and 3 requires an additional queue to handle the answerd value or the "finished" signal. Case 4 is invalid: it must be reassigned as case 2 or 3.

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

Some examples further explaining the above cases:

  3) Imagine a server processing requests that require some database queries. It, obviously, has an optimum number of threads to process such requests. Lets call this number: "n". A solution for this is to configure your server to handle "n" sockets at a time and you'll achieve the maximum throughput -- and you don't need this framework for that, provided all your clients will download the answers at the same speed. But what if you add some caching? Lets suppose that you queries will return the same results for the same service parameters. Now you will still have requests qurying the database, but not all "n" simultaneous connections will be doing it. By using this framework you can fine tune specific tasks like this and achive maximum throughput even if some clients are slow downloading your reply.

  4) Lower latency may be achieved when you are not at the maximum throughput -- the machine must have some idleness available. It happens by paralelizing requests. Imagine your server takes 3 independent database queries, from different databases. The traditional way is to execute Query1, Query2 and Query3 -- taking the time t1 + t2 + t3 to execute. If made parallel, it will take only max(t1, t2, t3). The pseudo code bellow illistrates the logic:

	q1Handle = reportConsumableEvent(QUERY1_EVENT, params1)
	q2Handle = reportConsumableEvent(QUERY2_EVENT, params2)
	q3Handle = reportConsumableEvent(QUERY3_EVENT, params3)
	(...)
	q1Answer1 = waitForAnswer(q1Handle)
	q2Answer2 = waitForAnswer(q2Handle)
	q3Answer3 = waitForAnswer(q3Handle)
	(...)
	reportAnswer(serviceAnwer)
	(...)
	// some other steps not needed by the client

  It is a good moment to remember that the traditional blocking method call with parameters & return values is not good for low latency solutions. Modern programs, nowadays, need to fullfil requisites not needed by their users (for instance, logging requests, computing & checking quotas, recording usage statistics, ...). Typically, when method calls that return values are used, you must wait for them to perform all their tasks in order to have access to their meaningful answer. The above line, "reportAnswer(serviceAnswer)", immediately makes the response available to the "caller". The benefits scale when that approach cascades.

  The following benchmark demonstrates the latency improvements: ...


API
===

    registerEventConsumer(MY_EVENT_TYPE, MY_METHOD)
    reportConsumableEvent(myEvent)
    eventId = reportConsumableEvent(QUERY3_EVENT, params3)
    eventId = produceEvent(MY_EVENT_TYPE, params)
    answer = waitForAnswer(myEvent)

    answer = waitForAnswer(myEvent)
    eventId = reportConsumableAnswerableEvent(MY_EVENT_TYPE, params)

    registerSynchronizableFunctionConsumer()
    registerSynchronizableProcedureConsumer()
    registerNonSynchronizableProcedureConsumer() -- this way, every event must hold the information

'EventLink's are responsible for linking producers & consumers and notifyers & observers. The following properties are controled:

  - Consumer Sub-Routine: can be functional ou procedural
  - Synchronicity: the events may be synchronizable or non-synchronizable
  - Medium: several extensions are possible. Some examples: direct (same thread), queue (different threads), network (remote)

    addListener(EventClient<EVENTS> listener)
    remoteListener(...)
    setConsumer(EventClient<EVENTS> listener)
    unsetConsumer()
    reportEvent(imi<EVENTS> events[])

EventLink<EventHandlerParamType, EventReturnParamType>


- https://nikitablack.github.io/2016/04/12/Generic-C-delegates.html
- https://www.codeproject.com/Articles/1170503/The-Impossibly-Fast-Cplusplus-Delegates-Fixed

Real-time requisites
====================

