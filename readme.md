Developed for the [PYNQ z2](https://www.amd.com/en/corporate/university-program/aup-boards/pynq-z2.html) and makes use of the [libpynq 5EWC0-2023-v0.2.6](https://pynq.tue.nl/libpynq/5EWC0-2023-v0.2.6/topics.html), it is part of [this project](https://github.com/alekseymorgunov/ryb). This code can sample a measured heartbeat signal and and detect its rate. It features a screen that displays each step of the algorithm data is graphed continuously from left to right:
- a red line plot shows the sampled input
- a blue bar plot shows the change between this averaged sample and the previous averaged sample
- a grey line shows represents the detection threshold
- if a blue bar exxceeds the grey line it turns green as well as its background.
