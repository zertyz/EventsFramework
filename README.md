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

This framework provides mechanisms for efficient producing and consuming (and also notifying and observing) events. Events can be "consumed" or "observed". For instance, when a server receives a request it needs a response. This is an e

Consumable Events may also be synchronous or asynchronous. Asynchronous events are the ones which you will not handle the answer. Synchronous, therefore, are the oposite.

	- An example of a "Synchronous Consumable Event" is the server response given above: the same socket that started the request must wait for the answer -- and the server thread must produce a consumable event and wait until there is an answer to be written back on the socket.
	- An "Asynchronous Consumable Event" example might be a notification event, lets say, for the system operators in case some error was detected: the server produces the event and resumes it's business. The "Event Consumer for the Server's Notification Events" might, on another thread, choose to send an email, sms, mobile push notification and so on. The event producer (the Server Internal Loop) don't need any answer from such events.

There are also Listenable Events. Those are events that work similarly to Consumable Events, but with some differences:

	- Listenable Events may have zero or more listeners, while Consumable Events should have exactly one consumer -- here we are refering to "listener methods" and "consumer methods", not threads.
	- Listenable Events may be ignored -- if there are no listeners and events are produced, they are discarded. When there are no consumers registered for a Consumable Events, they are enqueued or an error is thrown.
	- Listenable Events are not Synchronous nor Asynchronous for we don't care for any answer a listener might give us -- One can think of Listenable Events to be similar to "Asynchronous Consumable Event" in this regard

	- There is also the An "Asynchronous Consumable Event" example may be such server's mechanism for incrementing the usage metrics of their services: the event is generated with the needed information, but the main server code don't care if it will be listened by "Log Writters", "Metrics Gathering" or some sort of "DDoS Pattern Detection" algorithm.

Real-time requisites
====================

