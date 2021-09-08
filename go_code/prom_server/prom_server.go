package main

import (
    "math/rand"
    "net/http"
    "time"

    "github.com/prometheus/client_golang/prometheus"
    "github.com/prometheus/client_golang/prometheus/promauto"
    "github.com/prometheus/client_golang/prometheus/promhttp"
)

/* Creates a server listening on localhost 12345 and
 * has a gauge named "test_gauge", setting it to a
 * value of 12345 at first, then slowly incrementing it.
 */
func main() {
    var pMetricsPath string = "/metrics"
    var pListenAddress string = ":12345"

    pingGauge := promauto.NewGauge(prometheus.GaugeOpts{
        Name: "test_gauge",
        Help: "Gauge metrics over time",
    })

    pingGauge.Set(12345)

    go func() {
        for {
            pingGauge.Add(rand.Float64() * 0.0002)
            time.Sleep(2 * time.Second)
        }
    }()

    // Map Prometheus metrics scrape path to handler function
    http.Handle(pMetricsPath, promhttp.Handler())
    http.ListenAndServe(pListenAddress, nil)

    return
}

