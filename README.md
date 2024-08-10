# 9weather
weather widget for 9front that uses the [OpenWeatherMap API](https://openweathermap.org/).

![weather](screen.png)

## Keyboard shortcuts
q / Del to quit

## Usage

	9weather [ -d seconds ] [ -i ] [ -z zip,country ] [ -k apikey ]

9weather makes requests to OpenWeatherMap and such, requires
a API key in order to be used. You can register and get a
free key at http://openweathermap.org.

The default delay between each pull is 5 minutes, this can
be changed by setting the `-d` flag.

In order to display the temperature using imperial units add
the `-i` flag.

9weather reads the environment variables `openweathermap` and
`ZIP` to determine the API key and geolocation, they both can
be overwritten with the `-z` and `-k` flag respectively.

## License
MIT
