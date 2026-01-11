# Transport API Specification

## Overview
The transport API module fetches real-time departure information from the PRIM API (Île-de-France Mobilités) for configured transit stops.

## Data Source
- **API**: PRIM (Plateforme Régionale d'Information Multimodale)
- **Endpoint**: `https://prim.iledefrance-mobilites.fr/marketplace/stop-monitoring`
- **Format**: SIRI (Service Interface for Real-time Information) in JSON
- **Authentication**: API key in HTTP header (`apiKey`)

## Functional Requirements

### FR-T1: Fetch Departures
- Fetch next departures from a configured StopPoint/StopArea
- Parse SIRI JSON response to extract departure information
- Return structured departure data

### FR-T2: Multi-Station Support
- Support fetching from multiple stations in sequence
- Maintain order of stations as configured
- Handle each station type differently (RER vs BUS)

### FR-T3: Filtering
- **RER stations**: Filter by platform name (e.g., "2B")
- **BUS stations**: Filter by operator reference

### FR-T4: Data Extraction
For each departure, extract:
- Destination name (`JourneyNote[0].value`)
- Expected arrival/departure time
- Calculate delay in minutes from current time

### FR-T5: Error Handling
- Handle HTTP errors gracefully
- Handle JSON parsing errors
- Return empty list on failure (don't crash)
- Log errors to Serial for debugging

## Data Structure

```cpp
struct departureType {
    String name;      // Destination name
    String schedule;  // Formatted time (HH:MM)
    int delay;        // Minutes until departure
};
```

## Configured Stations

| Station | Type | MonitoringRef | Filter |
|---------|------|---------------|--------|
| RER C | RER | STIF:StopArea:SP:46366: | Platform "2B" |
| Bus 323 Molière | BUS | STIF:StopPoint:Q:39570: | Operator contains "MeC_Bus_PC:Operator::100:" |
| Bus 323 Jules Ferry | BUS | STIF:StopPoint:Q:39571: | Operator contains "MeC_Bus_PC:Operator::100:" |

## Refresh Rate
- Fetch new data every 30 seconds
- Use epoch timestamp comparison for timing

## Acceptance Criteria

- [ ] Successfully connects to PRIM API with valid API key
- [ ] Parses SIRI JSON response correctly
- [ ] Filters RER departures by platform
- [ ] Filters BUS departures by operator
- [ ] Returns up to 6 departures per station
- [ ] Calculates accurate delay in minutes
- [ ] Handles API errors without crashing
- [ ] Logs HTTP response codes for debugging
