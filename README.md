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

Consumable Events may also be synchronous or asynchronous. Asynchronous events are the ones that you will not handle the answer and synchronous, the oposite. An example of a Synchronous Consumable Event

Real-time requisites
====================

