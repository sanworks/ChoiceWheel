# ChoiceWheel

ChoiceWheel is a system for capturing rodent decisions, developed by Sanworks LLC: http://sanworks.io

It is a simplified adaptation of ChoiceBall:
http://jn.physiology.org/content/108/12/3416

To use the system, you need to 3D print the parts in the repository (note the included printing advice). You'll also need:
-Arduino Due
-A Yumo E6B2-CWZ3E rotary encoder:  https://www.sparkfun.com/products/11102
-An adhesive tape to secure the base to a stable surface

Getting Started

1. Connect the Arduino Due native USB port to your computer.

2. Upload the Arduino sketch in ChoiceWheel/Arduino/

3. Add /ChoiceWheel/MATLAB to the MATLAB path.

4. Download ArCOM from the Sanworks repository and add /ArCOM/MATLAB to the MATLAB path.

5. Download PsychToolbox from http://psychtoolbox.org/download/ 
and install it.

6. Restart MATLAB.

7. Create a ChoiceWheel object with:
W = ChoiceWheel('myPORT') where myPort is the name of Arduino Due's serial port.

8. Learn more about using the object on the ChoiceWheel wiki:
https://sites.google.com/site/choicewheelwiki/

Assistance with ChoiceBall is available in the Bpod forums:
https://sanworks.io/forum/forumdisplay.php?fid=1

Please send feedback to Sanworks at: support@sanworks.io