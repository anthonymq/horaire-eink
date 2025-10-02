// #include <Arduino.h>
// #include <iostream>
// using namespace std;
// struct departureType
// {
//     String name;
//     String schedule;
//     int delay;
// };// old 2.7 inch screen
// void drawScreen(std::vector<departureType> departures)
// {
//     display.setRotation(3);
//     display.setFont(&FreeSansBold12pt7b);
//     int16_t tbx, tby;
//     uint16_t tbw, tbh;
//     const char StationName[] = "RER C";
//     display.getTextBounds(StationName, 0, 0, &tbx, &tby, &tbw, &tbh);
//     // center the bounding box by transposition of the origin:
//     uint16_t x = ((display.width() - tbw) / 2) - tbx;

//     int16_t errorBoundX, errorBoundY;
//     uint16_t errorBoundW, errorBoundH;
//     display.getTextBounds("Aucun train", 0, 0, &errorBoundX, &errorBoundY, &errorBoundW, &errorBoundH);
//     // center the bounding box by transposition of the origin:
//     uint16_t errorMessageX = ((display.width() - errorBoundW) / 2) - errorBoundX;
//     uint16_t errorMessageY = ((display.height() - errorBoundH) / 2) + 13 - errorBoundY;

//     display.setFullWindow();
//     display.firstPage();
//     do
//     {
//         // HEADER
//         display.fillScreen(GxEPD_WHITE);

//         //// Header rectangle
//         display.fillRect(0, 0, 264, 44, GxEPD_BLACK);

//         // word headingXPos = 15;
//         display.setCursor(x, 30);
//         display.setTextColor(GxEPD_WHITE);
//         display.setFont(&FreeSansBold12pt7b);
//         display.print(StationName);
//         // /HEADER
//         // Error message

//         // /Error message
//         display.setTextColor(GxEPD_BLACK);
//         if (departures.size() > 0)
//         {
//             word dataYPos = 60;
//             word timeXPos = 10;
//             word timeYPos = dataYPos;
//             word destinationXPos = 80;
//             word destinationYPos = dataYPos;
//             word platformXPos = 180;
//             word platformYPos = dataYPos;
//             int verticalLineYPos = 66;
//             byte verticalSpacing = 23;

//             for (int i = 0; i < 6; i++)
//             {
//                 display.drawFastHLine(0, verticalLineYPos + verticalSpacing * i, 264, GxEPD_BLACK);
//             }
//             for (size_t i = 0; i < departures.size() && i < 6; i++)
//             {
//                 display.setFont(&FreeSansBold9pt7b);

//                 display.setCursor(timeXPos, timeYPos);
//                 display.print(departures[i].schedule);

//                 display.setCursor(destinationXPos, destinationYPos);
//                 display.print(departures[i].name);

//                 display.setCursor(platformXPos, platformYPos);
//                 char delayBuffer[6]; // XXXmin
//                 sprintf(delayBuffer, "%dmin", departures[i].delay);
//                 display.print(delayBuffer);

//                 // verticalLineYPos += verticalSpacing;
//                 timeYPos += verticalSpacing;
//                 destinationYPos += verticalSpacing;
//                 platformYPos += verticalSpacing;
//             }
//         }
//         else
//         {
//             display.setCursor(errorMessageX, errorMessageY);
//             display.print("Aucun train");
//         }
//     } while (display.nextPage());
// }
