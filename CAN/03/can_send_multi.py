import can
import time

can_msgs = [
    can.Message(arbitration_id=0x101, data=[0x01, 0x02, 0x03, 0x04], is_extended_id=True),
    can.Message(arbitration_id=0x102, data=[0x11, 0x12, 0x13, 0x14, 0x15], is_extended_id=False),
    can.Message(arbitration_id=0x103, data=[0x21, 0x22, 0x23, 0x24, 0x25, 0x26], is_extended_id=False),
]
can_bus = can.Bus(interface='kvaser', channel=1, bitrate=1000000)
while True:
    for can_msg in can_msgs:
        try:
            can_bus.send(can_msg)
            print(f"Message sent: {can_msg}")
        except can.CanError:
            print("Message NOT sent")
    time.sleep(0.1)