debug: hardware debugger for bc80 computer

It's based on Bluepill devboard (STM32F103C8T6) connected
with expansion header of bc80 via GPIOs and with PC via USB.

Main goal of project is accessing to RAM directly to upload and download
its content. Bluepill puts Z80 to permanent reset state and operates
with MREQ/RD/RW signals and Ax/Dx as well to read/write RAM contents.

# PC <-> Bluepill protocol

Bluepill acts as VCP device. Interaction is implemented as request - response sequence.
Only PC side sends requests and Bluepill responses after each request.

Request is one (or more) bytes message. Whole data stream can contain a lot of requests a time.
Bluepill should response to each of them as a single batch of responses as well.

Request/response formats are described in dbgproto.h header.
