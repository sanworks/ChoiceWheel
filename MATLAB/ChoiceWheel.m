%{
----------------------------------------------------------------------------

This file is part of the Sanworks Pulse Pal repository
Copyright (C) 2016 Sanworks LLC, Sound Beach, New York, USA

----------------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, version 3.

This program is distributed  WITHOUT ANY WARRANTY and without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
%}

% ChoiceWheel is a system to measure lateral paw sweeps in mice.
%
% Installation:
% 1. Install PsychToolbox from: http://psychtoolbox.org/download/
% 2. Install ArCOM from https://github.com/sanworks/ArCOM
% 
% - Create a ChoiceWheel object with W = ChoiceWheel('COMx') where COMx is your serial port string
% - Directly manipulate its fields to change trial parameters on the device.
% - Run W.stream to see streaming output (for testing purposes)
% - Run P = W.currentPosition to see the current wheel position (for testing purposes).
% - Run W.runTrial to start an experimental trial.
% - Check W.runningTrial to find out if the trial is still running (0 = idle, 1 = running)
% - Run data = W.lastTrialData once the trial is over, to return the trial outcome and wheel position record

classdef ChoiceWheel < handle
    properties
        Port % ArCOM Serial port
        idleTime2Start = 1; % Time with no ball motion needed to start a trial (s)
        idleTimeMotionGrace = 10; % Distance allowed during idle time (degrees)
        leftThreshold = 140; % Threshold for left choice (in degrees from animal's prespective, trials start at 180)
        rightThreshold = 220; % Threshold for right choice (in degrees)
        timeout = 5; % Time for choice response
        lastTrialData % Struct containing the last trial's data. The struct is empty until the trial is complete.
        runningTrial = 0; % 0 if idle, 1 if running a trial
        eventPinConfig = struct('gracePeriodPin', 8, 'trialStartPin', 9, 'leftChoicePin', 10, 'rightChoicePin', 11, 'timeoutPin', 12); % Arduino event pin configuration
    end
    properties (Access = private)
        acquiring = 0; % 0 if idle, 1 if acquiring data
        gui = struct; % Handles for GUI elements
        positionBytemask = logical(repmat([1 1 0 0 0 0], 1, 10000)); % For parsing data coming back from wheel
        timeBytemask = logical(repmat([0 0 1 1 1 1], 1, 10000));
        eventNames = {'Left', 'Right', 'Timeout'};
        nDisplaySamples = 1000; % When streaming to plot, show up to 1,000 samples
        maxDisplayTime = 10; % When streaming to plot, show up to last 10 seconds
        samplingTimer % A MATLAB timer object to check for incoming data periodically, so the command line is free during a trial
    end
    methods
        function obj = ChoiceWheel(portString)
            obj.Port = ArCOMObject(portString, 115200);
            obj.Port.write('C', 'uint8');
            response = obj.Port.read(1, 'uint8');
            if response ~= 217
                error('Could not connect =( ')
            end
            obj.samplingTimer = timer('TimerFcn',@(h,e)obj.readTrialData(), 'ExecutionMode', 'fixedRate', 'Period', 0.01);
        end
        function pos = currentPosition(obj)
            obj.Port.write('Q', 'uint8');
            pos = obj.Port.read(1, 'uint16');
        end
        function set.eventPinConfig(obj, eventPinConfig)
            obj.Port.write(['PE' eventPinConfig.gracePeriodPin eventPinConfig.trialStartPin...
                eventPinConfig.leftChoicePin eventPinConfig.rightChoicePin eventPinConfig.timeoutPin], 'uint8');
            obj.eventPinConfig = eventPinConfig;
        end
        function set.idleTime2Start(obj, initTime)
            obj.Port.write('PI', 'uint8', initTime*1000, 'uint32');
            obj.idleTime2Start = initTime;
        end
        function set.idleTimeMotionGrace(obj, graceTime)
            obj.Port.write('PG', 'uint8', graceTime*1000, 'uint16');
            obj.idleTimeMotionGrace = graceTime;
        end
        function set.leftThreshold(obj, thresh)
            obj.Port.write('PL', 'uint8', obj.degrees2pos(thresh), 'uint16');
            obj.leftThreshold = thresh;
        end
        function set.rightThreshold(obj, thresh)
            obj.Port.write('PR', 'uint8', obj.degrees2pos(thresh), 'uint16');
            obj.rightThreshold = thresh;
        end
        function set.timeout(obj, timeout)
            obj.Port.write('PT', 'uint8', timeout*1000, 'uint32');
            obj.timeout = timeout;
        end
        function Data = runTrial(obj)
            obj.Port.write('T', 'uint8');
            obj.runningTrial = 1;
            obj.lastTrialData = struct;
            start(obj.samplingTimer);
        end
        function stream(obj)
            obj.acquiring = 1;
            DisplayPositions = nan(1,obj.nDisplaySamples);
            DisplayTimes = nan(1,obj.nDisplaySamples);
            obj.gui.Fig  = figure('name','Position Stream','numbertitle','off', 'MenuBar', 'none', 'Resize', 'off', 'CloseRequestFcn', @(h,e)obj.endAcq());
            obj.gui.Plot = axes('units','normalized', 'position',[.2 .2 .65 .65]); ylabel('Position (deg)', 'FontSize', 18); xlabel('Time (s)', 'FontSize', 18);
            set(gca, 'xlim', [0 obj.maxDisplayTime], 'ylim', [0 360], 'ytick', [0 180 360]);
            Xdata = nan(1,obj.nDisplaySamples); Ydata = nan(1,obj.nDisplaySamples);
            obj.gui.StartLine = line([0,obj.maxDisplayTime],[180,180], 'Color', [.5 .5 .5]);
            obj.gui.OscopeDataLine = line([Xdata,Xdata],[Ydata,Ydata]);
            DisplayPos = 1;
            drawnow;
            obj.Port.write('S', 'uint8');
            SweepStartTime = 0;
            while obj.acquiring
                BytesAvailable = obj.Port.bytesAvailable;
                if BytesAvailable > 5
                    nBytesToRead = floor(BytesAvailable/6)*6;
                    Message = obj.Port.read(nBytesToRead, 'uint8');
                    nPositions = length(Message)/6;
                    Positions = typecast(Message(obj.positionBytemask(1:6*nPositions)), 'uint16');
                    Times = double(typecast(Message(obj.timeBytemask(1:6*nPositions)), 'uint32'))/1000;
                    DisplayTime = (Times(end)-SweepStartTime);
                    DisplayPos = DisplayPos + nPositions;
                    if DisplayTime >= obj.maxDisplayTime
                        DisplayPositions(1:DisplayPos) = NaN;
                        DisplayTimes(1:DisplayPos) = NaN;
                        DisplayPos = 1;
                        SweepStartTime = Times(end);
                    else
                        SweepTimes = Times-SweepStartTime;
                        DisplayPositions(DisplayPos-nPositions+1:DisplayPos) = obj.pos2degrees(Positions);
                        DisplayTimes(DisplayPos-nPositions+1:DisplayPos) = SweepTimes;
                    end
                    set(obj.gui.OscopeDataLine,'xdata',[DisplayTimes, DisplayTimes], 'ydata', [DisplayPositions, DisplayPositions]); drawnow;
                end
                pause(.0001);
            end
        end
        function delete(obj)
            obj.Port = []; % Trigger the ArCOM port's destructor function (closes and releases port)
        end
    end
    methods (Access = private)
        function obj = readTrialData(obj)
            if obj.Port.bytesAvailable % If trial data was returned
                obj.lastTrialData.PreTrialDuration = double(obj.Port.read(1, 'uint32'))/1000;
                obj.lastTrialData.nPositions = obj.Port.read(1, 'uint16');
                obj.lastTrialData.PosData = obj.Port.read(obj.lastTrialData.nPositions, 'uint16');
                obj.lastTrialData.TimeData = double(obj.Port.read(obj.lastTrialData.nPositions, 'uint32'))/1000;
                obj.lastTrialData.TerminatingEventCode = obj.Port.read(1, 'uint8');
                obj.lastTrialData.TerminatingEventName = obj.eventNames{obj.lastTrialData.TerminatingEventCode};
                obj.lastTrialData.TerminatingEventTime = obj.lastTrialData.TimeData(end);
                obj.runningTrial = 0;
                stop(obj.samplingTimer);
            end
        end
        function endAcq(obj)
            obj.Port.write('X', 'uint8');
            obj.acquiring = 0;
            delete(obj.gui.Fig);
            if obj.Port.bytesAvailable > 0
                obj.Port.read(obj.Port.bytesAvailable, 'uint8');
            end
        end
        function degrees = pos2degrees(obj, pos)
            degrees = (double(pos)/1024)*360;
        end
        function pos = degrees2pos(obj, degrees)
            pos = uint16((degrees/360)*1024);
        end
    end
end