# EventFramework
A framework for efficient (local or distributed) event driven programming, easing the creation of decoupled architectures and respecting real-time constraints.

Introduction
============

Event driven architectures aims to increase development productivity by allowing increased decoupling of components. The core concepts of Event Driven Programming are at the center of some serious and currently fashioned technologies:

  - Callbacks and reactive programming
  - Parallel computing
  - Distributed execution
  - Plugin architectures
  - Declarative Programming (related to the program's control flow, in oposition to Imperative Programming)

This framework provides mechanisms for efficient producing and consuming (and also notifying and observing) events. Events can be "consumed" or "observed". For instance, when a server receives a request it needs to provide a response -- this is an example of an event that was produced and consumed. On the other hand, when you plug a USB Stick on your computer, there might be a software to show the files: if there is, it will show; if there is not, nothing will happen. This is an example of an event who got notifyed but for which there may or may not be observers listening.

Consumable Events may also be synchronous or asynchronous. Asynchronous events are the ones which you will not handle the answer. Synchronous, therefore, are the ones which you want the answer.

  - An example of a "Synchronous Consumable Event" is the server response given above: the same socket that started the request must wait for the answer -- and the server thread must produce a consumable event, dispatch it to be processed somewhere and wait until there is an answer to be written back on the socket.
  - An "Asynchronous Consumable Event" example might be a notification event, lets say, for the system operators in case some error was detected: the server produces the event and resumes it's business. The "Event Consumer for the Server's Notification Events" might, on another thread, choose to send an email, sms, mobile push notification, just log it and so on. The event producer (the Server Internal Loop) don't need any answer from such events -- and these events should never be ignored as well: even it there are no consumers at the moment a Consumable Event is generated, this framework will keep track of it until one gets registered.

Now, back to Listenable Events: thet have some similarities to Consumable Events, but:

  - Listenable Events may have zero or more listeners, while Consumable Events should have exactly one consumer -- here we are refering to "listener methods" and "consumer methods", not threads.
  - Listenable Events might be ignored -- if there are no listeners and events are notifyed, they are discarded. Conversely, when there are no consumers registered for a Consumable Event, they are enqueued or an error is thrown.
  - Listenable Events are not Synchronous nor Asynchronous for we don't care for any answer a listener might give us -- One can think of Listenable Events to be similar to "Asynchronous Consumable Events" in this regard.

Sometimes there are cases in which it is difficult, by the definitions given above, to determina if we must consider an event a consumable or a listenable event:

  - Imagine the server's mechanism for incrementing the usage metrics of their services: the event is generated with the needed information, but the main server code don't care if it will be listened by "Log Writters", "Metrics Gathering" or some sort of "DDoS Pattern Detection" algorithm. This should be considered a Listenable Event, since an instance of such server might work without these features.

Real-time requisites
====================

