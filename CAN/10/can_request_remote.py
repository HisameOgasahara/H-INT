import can
import time

can_bus = can.interface.Bus(interface='kvaser', channel=1, bitrate=1000000)
rtr_msg = can.Message(arbitration_id=0x123, is_remote_frame=True, is_extended_id=False)

while True:
    try:
        can_bus.send(rtr_msg)
        print(f"Request Sent: {rtr_msg}")
        while True:
            msg = can_bus.recv()
            if msg.arbitration_id == 0x123 and not msg.is_remote_frame:
                print(f"Response Received: {msg}")
                break
    except can.CanError:
        print("Message NOT sent")
    time.sleep(1)