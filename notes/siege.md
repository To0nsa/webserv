# Siege Cheatsheet

## Basic Usage

```sh
siege [options] <url>
```

## ⚙️ Common Options

| Option             | Description                                 |
|--------------------|---------------------------------------------|
| `-c <num>`         | Number of concurrent users (clients)       |
| `-r <num>`         | Repetitions per user                        |
| `-t <time>`        | Time to run (e.g., 1m, 30s)               |
| `-d <num>`         | Delay between requests (random 0-N seconds)|
| `-f <file>`        | Read URLs from file                         |
| `-i`               | Random URL hit from file (-f)              |
| `-b`               | No delay (benchmark mode)                   |
| `--header "KEY: VAL"` | Add custom HTTP header                   |
| `--content-type`   | Set Content-Type header                     |
| `-H`               | Same as --header                            |
| `-m "msg"`         | Log message                                 |
| `-l`               | Log output to `~/siege.log`                |
| `-v`               | Verbose output                              |
| `-q`               | Quiet output                                |

## 🔁 Examples

### Basic test:
```sh
siege http://localhost:8080
```

### Run for 30 seconds with 10 users:
```sh
siege -c 10 -t 30s http://localhost:8080
```

### 50 users, 10 repetitions:
```sh
siege -c50 -r10 http://localhost:8080
```

### Hit URLs from a file randomly:
```sh
siege -c 20 -r 5 -i -f urls.txt
```

**urls.txt content example:**
```
http://example.com/page1
http://example.com/page2
http://example.com/page3
```

### POST JSON data with custom headers:
```sh
siege -c 5 -r 10 \
  --header "Content-Type: application/json" \
  'http://localhost:5000 POST {"username":"test","password":"1234"}'
```

### Benchmark mode (no delay):
```sh
siege -b -c 50 -r 100 http://localhost
```

### Run for 1 minute:
```sh
siege -t1M http://localhost:8080
```

### Test multiple URLs from file:
```sh
siege -f urls.txt
```

## Useful Links

- [Siege Documentation](https://www.joedog.org/siege-manual/)
- [Siege GitHub](https://github.com/JoeDog/siege)
