import os
import os.path
import argparse
import conf

from saleae import automation
from datetime import datetime

parser = argparse.ArgumentParser(description="Capture data using a logic analyzer")
parser.add_argument("-d", "--duration", type=float, default=5.0, help="Time duration for recording the data")
args = parser.parse_args()

if(args.duration <= 0):
    parser.error("duration must be positive")

SERIAL_NUMBER = "B6E9DB9B1B9568F7"

'''
Connect to the running Logic 2 Application on port 10430.
Using the "with" statement will automatically call manager.close() 
when exiting the scope.
'''

with automation.Manager.connect(port=10430) as manager:
    '''
    Configure the capturing device to record on digital channels 3, 5 and 7
    with a sampling rate of 4 MSa/s, and logic level of 3.0V.
    '''
    device_configuration = automation.LogicDeviceConfiguration(
        enabled_digital_channels=conf.digital_channels,
        digital_sample_rate=conf.sampling_rate,
    )

    # Record 5 seconds of data before stopping the capture
    capture_configuration = automation.CaptureConfiguration(
        capture_mode=automation.TimedCaptureMode(duration_seconds=args.duration)
    )

    '''
       Start a capture - the capture will be automatically closed when leaving 
       the `with` block
              To use a real device, you can:
                1. Omit the `device_id` argument. Logic 2 will choose 
                    the first real (non-simulated) device.
                2. Use the serial number for your device. See the "Finding the 
                    Serial Number of a Device" section for information on finding 
                    your device's serial number.    
    '''

    with manager.start_capture(
            #device_id=SERIAL_NUMBER,
            device_configuration=device_configuration,
            capture_configuration=capture_configuration) as capture:

        # Wait until the capture has finished
        # This will take about 5 seconds because we are using a timed capture mode
        capture.wait()

        # Store output in a timestamped directory
        output_dir = os.path.join(os.getcwd(), f'output-{datetime.now().strftime("%Y-%m-%d_%H-%M-%S")}')
        os.makedirs(output_dir)

        # Export raw digital data (GPIO transitions) to a CSV file
        capture.export_raw_data_csv(directory=output_dir, digital_channels=device_configuration.enabled_digital_channels)

        # Finally, save the capture to a file
        capture_filepath = os.path.join(output_dir, 'example_capture.sal')
        capture.save_capture(filepath=capture_filepath)