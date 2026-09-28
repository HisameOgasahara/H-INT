import can

can_bus = can.Bus(interface='kvaser', channel=0, bitrate=1000000)
response_data = [0, 25, 0, 1, 3, 1, 4, 1]

for msg in can_bus:
    if msg.is_remote_frame and msg.arbitration_id == 0x123:
        print(f"Request Received: {msg}")
        response_msg = can.Message(arbitration_id=0x123, data=response_data, is_extended_id=False)
        try:
            can_bus.send(response_msg)
            print(f"Data sent: {response_msg}")
        except can.CanError:
            print("Message NOT sent")