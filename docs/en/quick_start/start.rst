Getting Started Guide
=========================

First of all, welcome to the world of USB. Here you can learn various USB knowledge and CherryUSB porting, usage, advanced features, etc. However, as a newcomer, you must be quite confused because USB is difficult (actually, once you learn CherryUSB, you'll find that USB is not difficult at all). So in this situation, what should your learning path be? Here, I recommend following my learning path, as this will be most helpful for your USB growth and you won't give up midway.

First, don't start by looking at concepts. There's an ancient saying: **"What you read on paper is always shallow; to truly understand, you must practice"**. Just looking at written materials won't teach you much. Only when you practice yourself can you gain deeper understanding of these concepts. So as a beginner, what should you do? Please follow these steps.


Step 1
-------------

You need to have learned C language, UART, and DMA - these are the basics. If you haven't learned them, please go learn them first, otherwise you'll struggle. You might ask: what's the relationship between USB and UART/DMA? I can only say two words: **equivalent**.

Step 2
-------------

Download demo projects and get them running. **For slow learners, I recommend using the same chip model as the demo**. For fast learners, you can choose to port to supported chip models yourself. If you can't even get the demos running, what USB can you learn? Don't you agree?

Step 3
---------

Excellent! By this step, you can already skillfully port and run all examples. So what should you learn next? **Transactions**, **Requests**, and **Descriptors** (in your USB learning journey, you only need to know these three things; you don't need to know anything else).

Step 4
----------

First, we need to know that USB transactions include SETUP/IN/OUT, which are essentially equivalent to sending commands, sending data, and receiving data - very simple. As for the control phase, data phase, and status phase that you hear about in online discussions about enumeration, they are not transactions themselves; they are just multiple transactions representing a phase.

Step 5
----------

Then look at the **USB Enumeration** section and learn about the concept of **descriptors**. At this point, you can briefly look at what descriptors are and what types exist. You need to remember and memorize **the composition of device, configuration, interface, and endpoint descriptors**. You don't need to know anything else because everything else is fixed and can be copy-pasted later. There are enumeration captures for various devices in the group files that you can download and review.

Step 6
----------

Then you can look at what **requests** are, the composition of request structures, and what types of requests exist - just a basic understanding will suffice. Why? Because it's just an 8-byte data format. Everyone can write UART + custom protocol, and USB requests are the same, just predefined.

Step 7
----------

At this point, you should familiarize yourself with some protocol stack APIs by referring to the **API Manual** section. You also need to know what conditions constitute interrupt completion, when reception is considered complete, and when transmission is considered complete. You can refer to the **USB Knowledge Extension** section.

Step 8
----------

By this step, you're definitely very knowledgeable, and you can start working on some small functional projects. During this period, please repeatedly review the **USB Knowledge Extension** section until you truly understand it, because this content is very important and will affect the execution results of our code.

Step 9
----------

You've reached this point - you shouldn't need me anymore! Now you can look at USB concepts, USB details, and CherryUSB code flow. Then it's just consolidation, consolidation, and more consolidation. Congratulations, you've graduated!
