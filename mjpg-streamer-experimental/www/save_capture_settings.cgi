#!/bin/bash
#
#
#
#                      brightness 0x00980900 (int)    : min=-128 max=127 step=1 default=0 value=10 flags=slider
#                      contrast 0x00980901 (int)    : min=0 max=255 step=1 default=128 value=70 flags=slider
#                    saturation 0x00980902 (int)    : min=0 max=255 step=1 default=128 value=128 flags=slider
#                           hue 0x00980903 (int)    : min=-127 max=128 step=1 default=0 value=0 flags=slider
#
#
# Parse and sanitize arguments, then save to a file in /home/pi.
#
# If you change CAPS, be sure to change it in capture_settings.sh as well.
CAPS=/boot/capture_settings.txt
#b=21&c=214&s=128&h=0
SETTINGS=$(echo $QUERY_STRING | sed -e 's/&/ /g')
eval $SETTINGS
BRIGHTNESS=$b
CONTRAST=$c
SATURATION=$s
HUE=$h

  if ! [ "$BRIGHTNESS" -eq "$BRIGHTNESS" ] 2>/dev/null
  then
       printf "Brightness must be an integer\n"
       exit 1
  fi
  if [ "$BRIGHTNESS" -lt -128 -o "$BRIGHTNESS" -gt 127 ]
  then
       printf "Brightness must be gt -128 and lt 127 (%d)\n" $BRIGHTNESS
       exit 1
  fi
  if ! [ "$CONTRAST" -eq "$CONTRAST" ] 2>/dev/null
  then
       printf "Contrast must be an integer\n"
       exit 1
  fi
  if [ "$CONTRAST" -lt 0 -o "$CONTRAST" -gt 255 ]
  then
       printf "Contrast must be gt 0 and lt 255 (%d)\n" $CONTRAST
       exit 1
  fi
  if ! [ "$SATURATION" -eq "$SATURATION" ] 2>/dev/null
  then
       printf "Saturation must be an integer\n"
       exit 1
  fi
  if [ "$SATURATION" -lt 0 -o "$SATURATION" -gt 255 ]
  then
       printf "Saturation must be gt 0 and lt 255 (%d)\n" $SATURATION
       exit 1
  fi
  if ! [ "$HUE" -eq "$HUE" ] 2>/dev/null
  then
       printf "Hue must be an integer\n"
       exit 1
  fi
  if [ "$HUE" -lt -128 -o "$HUE" -gt 127 ]
  then
       printf "Hue must be gt -128 and lt -127 (%d)\n" $HUE
       exit 1
  fi

mount /boot
echo BRIGHTNESS=$BRIGHTNESS > $CAPS
echo CONTRAST=$CONTRAST >> $CAPS
echo SATURATION=$SATURATION >> $CAPS
echo HUE=$HUE >> $CAPS
umount /boot
echo
exit 0
