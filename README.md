# 9weather
weather widget for 9front that uses the [OpenWeatherMap API](https://openweathermap.org/).

![weather](screen.png)

## Keyboard shortcuts
q / Del to quit

## Usage

	9weather [ -d seconds ] [ -i ] [ -z zip,country ] [ -k apikey ]

9weather retrieves weather data from OpenWeatherMap and
requires an API key for access.  You can easily obtain a
free API key by registering at http://openweathermap.org/

By default, 9weather fetches weather data every 5 minutes.
This interval can be changed by setting the -d flag, fol-
lowed by the desired delay in seconds.

9weather normally displays the temperature in metric units,
to display in imperial units add the -i flag.

9weather uses the environment variable openweathermap to
obtain the API key and ZIP to determine the geolocation,
they both can be overwritten with -z and -k flag respec-
tively.

## License
MIT
