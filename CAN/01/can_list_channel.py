import can

channels = can.detect_available_configs(interfaces='kvaser')
print("Available CAN channels:")
for channel in channels:
    print(f"- {channel}")