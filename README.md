# grbl_pause_via_tiny
Program to use an ATtiny85 to control the HOLD, RESUME, and ABORT pins on GRBL.

GRBL has far too much to handle to worry about the user's intentions regarding HOLD/RESUME/ABORT.  That's why it's OK for GRBL to
have 3 pins dedicated to interrupting (or resuming) processing.  This project takes a single ATtiny85 hooked up to a single button
on B0.

When the button is pressed for 100ms, the HOLD signal (attached to B1) is pulled low for 250ms.

If the button is pressed for 1s, the ABORT signal (attached to B3) is pulled low for 250ms.

If the button is pressed for less than 1s and has been used to trigger HOLD twice since the last ABORT or RESUME signal,
then the RESUME signal (attached to B2) is pulled low for 250ms.

So if you press the button, HOLD is triggered after 100ms
If you press the button again, HOLD is triggered after 100ms and then RESUME is triggered when the button is released.
Press again, you just get HOLD.
Press again, you get HOLD and RESUME.
etc.

If you press the button for 1s, ABORT is triggered and RESUME will not be triggered when the button is released.

It would have been trivial to avoid the extra HOLD signals.  However, GRBL could have resumed via software and we don't know that.
If the user now wants to abort, the system would not respond for 1 second, which could cause damage to product or machine.
By triggering the second HOLD, we now stop the system 90% faster, hopefully limiting damage, and if the user continues holding 
we trigger the ABORT.

## license
This program is released under the MIT license.
Copyright (C) 2017 Beau Barker (https://github.com/barkerest)
