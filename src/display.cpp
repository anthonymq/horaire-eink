#include "display.h"
#include "weathericons.h"
#define USE_SERIAL Serial

// Draw a 2-bit PROGMEM weather icon as monochrome on the display
// 2-bit values: 0=white, 1=light gray (white), 2=dark gray (black), 3=black
// The icon data has a 6-byte header: version(1) + bitdepth(1) + width(2) + height(2)
void draw2BitIcon(int x, int y, const char* iconData, int width, int height, uint16_t color) {
    const int HEADER_SIZE = 6;
    // In 2-bit mode, each byte contains 4 pixels (2 bits each)
    // Row width is padded to the next byte boundary
    int paddedWidth = ((width + 3) / 4) * 4;  // Round up to next multiple of 4 pixels
    int bytesPerRow = paddedWidth / 4;        // 4 pixels per byte at 2 bits each
    
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            // Calculate byte position in the row
            int byteIndex = HEADER_SIZE + (row * bytesPerRow) + (col / 4);
            // Calculate bit position within byte (4 pixels per byte, 2 bits each, MSB first)
            int pixelInByte = 3 - (col % 4);  // 3,2,1,0 for pixels 0,1,2,3
            int shift = pixelInByte * 2;
            
            // Read byte from PROGMEM and extract 2-bit value
            uint8_t byteVal = pgm_read_byte(iconData + byteIndex);
            uint8_t pixelVal = (byteVal >> shift) & 0x03;
            
            // Map 2-bit grayscale to monochrome: 0,1 = white (skip), 2,3 = black (draw)
            if (pixelVal >= 2) {
                display.drawPixel(x + col, y + row, color);
            }
        }
    }
}

void displayBanner(int posx, int posy, int height, int width, String text)
{
    // HEADER
    display.setFont(&FreeSansBold12pt7b);
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(text, posx, posy, &tbx, &tby, &tbw, &tbh);
    // center the bounding box by transposition of the origin:
    uint16_t x = ((width - tbw) / 2) - tbx;
    uint16_t y = ((height - tbh) / 2) - tby;
    do
    {
        //// Header rectangle
        display.fillRect(0, 0, 300, 44, GxEPD_BLACK);

        // word headingXPos = 15;
        display.setCursor(x, 30);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        display.print(text);
        // /HEADER

    } while (display.nextPage());
}

// Add this new function to display.cpp

void displayWeather(int posx, int posy, int width, int height, weatherType weather) {
    // Draw a box for the weather information
    display.drawRect(posx, posy, width, height, GxEPD_BLACK);
    
    // Set font and color
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    
    // Display city name
    display.setCursor(posx + 5, posy + 20);
    display.print("Ivry-sur-Seine");
    
    // Display temperature
    display.setCursor(posx + 5, posy + 40);
    display.print(String(weather.temperature, 1) + "C");
    
    // Display feels like
    display.setCursor(posx + 5, posy + 60);
    display.print("Ressenti: " + String(weather.feels_like, 1) + "C");
    
    // Display humidity
    display.setCursor(posx + 5, posy + 80);
    display.print("Humidite: " + String(weather.humidity) + "%");
    
    // Display description
    display.setCursor(posx + 5, posy + 100);
    display.print(weather.description);
}

// Modify the drawScreen function to include weather display
void drawScreen(std::vector<std::vector<departureType>> allDepartures, weatherType weather, bool fullRefresh)
{
    display.setRotation(3);

    if (fullRefresh)
    {
        display.setFullWindow();
    }
    else
    {
        display.setPartialWindow(0, 0, display.width(), display.height());
    }

    display.firstPage();
    display.setTextColor(GxEPD_BLACK);

    do
    {
        // Constants for departure board display
        const int BANNER_HEIGHT = 44;
        const int WEATHER_HEIGHT = 55; // Increased to fit 50px icon
        const int ROWS = 5;
        const int PADDING = 5;
        
        // Weather icon dimensions (from weathericons.h mini meteocons)
        const int ICON_WIDTH = 50;
        const int ICON_HEIGHT = 50;
        
        // Clear the screen with white background
        display.fillScreen(GxEPD_WHITE);
        
        // Display weather information at the top
        display.setFont(&FreeSansBold9pt7b);
        display.setTextColor(GxEPD_BLACK);
        
        // Draw the weather icon using 2-bit to monochrome conversion
        const char* iconData = getMiniMeteoconIconFromProgmem(weather.icon);
        if (iconData != nullptr) {
            draw2BitIcon(PADDING, 3, iconData, ICON_WIDTH, ICON_HEIGHT, GxEPD_BLACK);
        }
        
        // Position weather text next to the icon
        int textX = PADDING + ICON_WIDTH + 10;
        int textY = 25; // Vertically centered in weather bar
        display.setCursor(textX, textY);
        display.print(String(weather.temperature, 1) + " C");
        
        // Second line: humidity
        display.setCursor(textX, textY + 20);
        display.print(String(weather.humidity) + "% hum");
        
        // Draw a separator line below weather bar
        display.drawFastHLine(0, WEATHER_HEIGHT, display.width(), GxEPD_BLACK);
        
        // Draw main board (top half)
        {
            // Main board parameters
            int posx = 0;
            int posy = WEATHER_HEIGHT; // Start after weather info
            int height = (display.height() / 2) - WEATHER_HEIGHT;
            int width = display.width();
            String banner = "RER C";
            bool showNames = true;
            
            // Draw banner
            display.setFont(&FreeSansBold12pt7b);
            int16_t tbx, tby;
            uint16_t tbw, tbh;
            display.getTextBounds(banner.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            uint16_t x = posx + ((width - tbw) / 2);
            

            // display.fillRect(posx, posy, width, BANNER_HEIGHT, GxEPD_BLACK);
            display.setCursor(x, posy + 30);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold12pt7b);
            display.print(banner);
            
            // Adjust position and height for the departure board
            int boardPosy = posy + BANNER_HEIGHT;
            int boardHeight = height - BANNER_HEIGHT;
            int cellHeight = boardHeight / ROWS;
            
            // Calculate error message position
            int16_t errorBoundX, errorBoundY;
            uint16_t errorBoundW, errorBoundH;
            display.getTextBounds("Aucun train", 0, 0, &errorBoundX, &errorBoundY, &errorBoundW, &errorBoundH);
            uint16_t errorMessageX = posx + ((width - errorBoundW) / 2) - errorBoundX;
            uint16_t errorMessageY = boardPosy + ((boardHeight - errorBoundH) / 2) - errorBoundY;
            
            display.drawRect(posx, boardPosy, width, boardHeight, GxEPD_BLACK);
            
            if (!allDepartures.empty() && !allDepartures[0].empty())
            {
                // Draw horizontal grid lines
                for (int i = 1; i < ROWS; i++)
                {
                    display.drawFastHLine(posx, boardPosy + (i * cellHeight), width, GxEPD_BLACK);
                }
                
                // Display departures
                for (size_t i = 0; i < allDepartures[0].size() && i < ROWS; i++)
                {
                    int cellY = boardPosy + (i * cellHeight) + 20;
                    int16_t tbx, tby;
                    uint16_t tbw, tbh;
                    
                    display.setTextColor(GxEPD_BLACK);
                    display.setFont(&FreeSansBold9pt7b);
                    
                    // Schedule (left)
                    display.setCursor(posx + PADDING, cellY);
                    display.print(allDepartures[0][i].schedule);
                    
                    if (showNames)
                    {
                        // Name (center)
                        display.getTextBounds(allDepartures[0][i].name, 0, 0, &tbx, &tby, &tbw, &tbh);
                        display.setCursor(posx + (width - tbw) / 2, cellY);
                        display.print(allDepartures[0][i].name);
                    }
                    
                    // Delay (right)
                    char delayBuffer[6];
                    sprintf(delayBuffer, "%dmin", allDepartures[0][i].delay);
                    display.getTextBounds(delayBuffer, 0, 0, &tbx, &tby, &tbw, &tbh);
                    display.setCursor(posx + width - tbw - PADDING, cellY);
                    display.print(delayBuffer);
                }
            }
            else
            {
                display.setCursor(errorMessageX, errorMessageY);
                display.print("Aucun train");
            }
        }
        
        // Draw bottom left board
        {
            // Bottom left board parameters
            int posx = 0;
            int posy = display.height() / 2;
            int height = display.height() / 2;
            int width = display.width() / 2;
            String banner = "323 Moliere";
            bool showNames = false;
            std::vector<departureType> departures;
            
            // Safely get departures if available
            if (allDepartures.size() > 1 && allDepartures[1].size() > 0) {
                departures = allDepartures[1];
            }
            
            // Draw banner
            display.setFont(&FreeSansBold12pt7b);
            int16_t tbx, tby;
            uint16_t tbw, tbh;
            display.getTextBounds(banner.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            uint16_t x = posx + ((width - tbw) / 2);
            
            display.fillRect(posx, posy, width, BANNER_HEIGHT, GxEPD_BLACK);
            display.setCursor(x, posy + 30);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold12pt7b);
            display.print(banner);
            
            // Adjust position and height for the departure board
            int boardPosy = posy + BANNER_HEIGHT;
            int boardHeight = height - BANNER_HEIGHT;
            int cellHeight = boardHeight / ROWS;
            
            // Calculate error message position
            int16_t errorBoundX, errorBoundY;
            uint16_t errorBoundW, errorBoundH;
            display.getTextBounds("Aucun train", 0, 0, &errorBoundX, &errorBoundY, &errorBoundW, &errorBoundH);
            uint16_t errorMessageX = posx + ((width - errorBoundW) / 2) - errorBoundX;
            uint16_t errorMessageY = boardPosy + ((boardHeight - errorBoundH) / 2) - errorBoundY;
            
            display.drawRect(posx, boardPosy, width, boardHeight, GxEPD_BLACK);
            
            if (!departures.empty())
            {
                // Draw horizontal grid lines
                for (int i = 1; i < ROWS; i++)
                {
                    display.drawFastHLine(posx, boardPosy + (i * cellHeight), width, GxEPD_BLACK);
                }
                
                // Display departures
                for (size_t i = 0; i < departures.size() && i < ROWS; i++)
                {
                    int cellY = boardPosy + (i * cellHeight) + 20;
                    int16_t tbx, tby;
                    uint16_t tbw, tbh;
                    
                    display.setTextColor(GxEPD_BLACK);
                    display.setFont(&FreeSansBold9pt7b);
                    
                    // Schedule (left)
                    display.setCursor(posx + PADDING, cellY);
                    display.print(departures[i].schedule);
                    
                    if (showNames)
                    {
                        // Name (center)
                        display.getTextBounds(departures[i].name, 0, 0, &tbx, &tby, &tbw, &tbh);
                        display.setCursor(posx + (width - tbw) / 2, cellY);
                        display.print(departures[i].name);
                    }
                    
                    // Delay (right)
                    char delayBuffer[6];
                    sprintf(delayBuffer, "%dmin", departures[i].delay);
                    display.getTextBounds(delayBuffer, 0, 0, &tbx, &tby, &tbw, &tbh);
                    display.setCursor(posx + width - tbw - PADDING, cellY);
                    display.print(delayBuffer);
                }
            }
            else
            {
                display.setCursor(errorMessageX, errorMessageY);
                display.print("Aucun train");
            }
        }
        
        // Draw bottom right board
        {
            // Bottom right board parameters
            int posx = display.width() / 2;
            int posy = display.height() / 2;
            int height = display.height() / 2;
            int width = display.width() / 2;
            String banner = "323 Jules F";
            bool showNames = false;
            std::vector<departureType> departures;
            
            // Safely get departures if available
            if (allDepartures.size() > 2 && allDepartures[2].size() > 0) {
                departures = allDepartures[2];
            }
            
            // Draw banner
            display.setFont(&FreeSansBold12pt7b);
            int16_t tbx, tby;
            uint16_t tbw, tbh;
            display.getTextBounds(banner.c_str(), 0, 0, &tbx, &tby, &tbw, &tbh);
            uint16_t x = posx + ((width - tbw) / 2);
            
            display.fillRect(posx, posy, width, BANNER_HEIGHT, GxEPD_BLACK);
            display.setCursor(x, posy + 30);
            display.setTextColor(GxEPD_WHITE);
            display.setFont(&FreeSansBold12pt7b);
            display.print(banner);
            
            // Adjust position and height for the departure board
            int boardPosy = posy + BANNER_HEIGHT;
            int boardHeight = height - BANNER_HEIGHT;
            int cellHeight = boardHeight / ROWS;
            
            // Calculate error message position
            int16_t errorBoundX, errorBoundY;
            uint16_t errorBoundW, errorBoundH;
            display.getTextBounds("Aucun train", 0, 0, &errorBoundX, &errorBoundY, &errorBoundW, &errorBoundH);
            uint16_t errorMessageX = posx + ((width - errorBoundW) / 2) - errorBoundX;
            uint16_t errorMessageY = boardPosy + ((boardHeight - errorBoundH) / 2) - errorBoundY;
            
            display.drawRect(posx, boardPosy, width, boardHeight, GxEPD_BLACK);
            
            if (!departures.empty())
            {
                // Draw horizontal grid lines
                for (int i = 1; i < ROWS; i++)
                {
                    display.drawFastHLine(posx, boardPosy + (i * cellHeight), width, GxEPD_BLACK);
                }
                
                // Display departures
                for (size_t i = 0; i < departures.size() && i < ROWS; i++)
                {
                    int cellY = boardPosy + (i * cellHeight) + 20;
                    int16_t tbx, tby;
                    uint16_t tbw, tbh;
                    
                    display.setTextColor(GxEPD_BLACK);
                    display.setFont(&FreeSansBold9pt7b);
                    
                    // Schedule (left)
                    display.setCursor(posx + PADDING, cellY);
                    display.print(departures[i].schedule);
                    
                    if (showNames)
                    {
                        // Name (center)
                        display.getTextBounds(departures[i].name, 0, 0, &tbx, &tby, &tbw, &tbh);
                        display.setCursor(posx + (width - tbw) / 2, cellY);
                        display.print(departures[i].name);
                    }
                    
                    // Delay (right)
                    char delayBuffer[6];
                    sprintf(delayBuffer, "%dmin", departures[i].delay);
                    display.getTextBounds(delayBuffer, 0, 0, &tbx, &tby, &tbw, &tbh);
                    display.setCursor(posx + width - tbw - PADDING, cellY);
                    display.print(delayBuffer);
                }
            }
            else
            {
                display.setCursor(errorMessageX, errorMessageY);
                display.print("Aucun train");
            }
        }
        
        // Draw the dividing lines for the bottom boards
        display.drawRect(0, display.height() / 2, display.width() / 2, display.height() / 2, GxEPD_BLACK);
        display.drawRect(display.width() / 2, display.height() / 2, display.width() / 2, display.height() / 2, GxEPD_BLACK);
        
    } while (display.nextPage());
}

void displayLoader(String message)
{
    display.setRotation(3);
    display.setFont(&FreeSansBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(message, 0, 0, &tbx, &tby, &tbw, &tbh);
    // center the bounding box by transposition of the origin:
    uint16_t x = ((display.width() - tbw) / 2) - tbx;
    uint16_t y = ((display.height() - tbh) / 2) - tby;
    display.setFullWindow();
    display.firstPage();
    do
    {
        display.fillScreen(GxEPD_WHITE);
        display.setCursor(x, y);
        display.print(message);
    } while (display.nextPage());
}

void displayDataFreshness(bool success, long lastSuccess, long currentEpoch)
{
    USE_SERIAL.println("currentEpoch");
    USE_SERIAL.println(currentEpoch);
    if (success)
    {
        // lcd.setCursor(12, 0);
        // lcd.print("    ");
        // lcd.setCursor(12, 0);
        // // lcd.print(lastSuccess);
        // lcd.setCursor(15, 0);
        // lcd.printByte(5);
    }
    else
    {
        // lcd.setCursor(12, 0);
        // lcd.print("    ");
        // lcd.setCursor(12, 0);
        // lcd.printByte(6);
        // lcd.print((now - lastSuccess) / 60);
        // lcd.print("m");
    }
}