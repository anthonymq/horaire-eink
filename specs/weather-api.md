# Weather API Specification

## Overview
The weather API module fetches current weather conditions from OpenWeatherMap for display on the E-Ink screen.

## Data Source
- **API**: OpenWeatherMap Current Weather
- **Endpoint**: `https://api.openweathermap.org/data/2.5/weather`
- **Format**: JSON
- **Authentication**: API key in query parameter (`appid`)

## Functional Requirements

### FR-W1: Fetch Current Weather
- Fetch current weather for configured city
- Use metric units (Celsius)
- Parse JSON response to extract weather data

### FR-W2: Data Extraction
Extract from API response:
- Temperature (`main.temp`)
- Feels-like temperature (`main.feels_like`)
- Humidity percentage (`main.humidity`)
- Weather description (`weather[0].description`)
- Weather icon code (`weather[0].icon`)
- Wind speed (`wind.speed`)

### FR-W3: Icon Mapping
Map OpenWeatherMap icon codes to local weather icons:

| OWM Code | Condition | Local Icon |
|----------|-----------|------------|
| 01d/01n | Clear sky | sunny |
| 02d/02n | Few clouds | partlycloudy |
| 03d/03n | Scattered clouds | mostlycloudy |
| 04d/04n | Broken clouds | cloudy |
| 09d/09n | Shower rain | chancerain |
| 10d/10n | Rain | rain |
| 11d/11n | Thunderstorm | tstorms |
| 13d/13n | Snow | snow |
| 50d/50n | Mist/Fog | fog |

### FR-W4: Error Handling
- Handle HTTP errors gracefully
- Handle JSON parsing errors
- Return default/empty weather struct on failure
- Log errors to Serial for debugging

## Data Structure

```cpp
struct weatherType {
    float temperature;    // Current temperature in Celsius
    float feels_like;     // Feels-like temperature
    int humidity;         // Humidity percentage (0-100)
    String description;   // Weather description text
    String icon;          // Mapped icon name for display
    float wind_speed;     // Wind speed in m/s
};
```

## Configuration

| Parameter | Value |
|-----------|-------|
| City | Ivry-sur-Seine |
| Units | metric |
| Language | (default - English) |

## Refresh Rate
- Fetch new weather data every 30 minutes (1800 seconds)
- Weather changes slowly; frequent updates waste API calls

## Acceptance Criteria

- [ ] Successfully connects to OpenWeatherMap API with valid key
- [ ] Parses JSON response correctly
- [ ] Extracts all required weather fields
- [ ] Maps OWM icon codes to local icon names
- [ ] Returns valid weather struct with data
- [ ] Handles API errors without crashing
- [ ] Respects 30-minute refresh interval
- [ ] Logs weather data for debugging
