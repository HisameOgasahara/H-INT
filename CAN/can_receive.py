import	can

can_bus	= can.Bus(interface='kvaser', channel=0, bitrate=1000000)
while	True:
    can_msg	=	can_bus.recv()
    if	can_msg:
        print(f"Message	received:{can_msg}")