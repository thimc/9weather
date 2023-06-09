# 9weather

9weather forecast widget for 9front that uses the [OpenWeatherMap API](https://openweathermap.org/).

![weather](screen.png)

## Keyboard shortcuts

q / Del to quit

## Usage

In order to use 9weather you must obtain a free API key from OpenWeatherMap.

`usage: 9weather [-d delay] [-i] [-z zip,country] [-f font] [-k apikey]`


The free plan allows the user to fetch data 2000 times per day which should be\
sufficient for this program.

You need to supply your api key directly via the `-k` argument.

The default delay between each pull is 5 minutes, this can be changed with the\
`-d` parameter.  Note that this delay is in milliseconds.

The default behaviour is to display the temperature in Celsius, this can be\
changed by setting the `-i` parameter (for imperial units).

## License

MIT

