# Python REPL Server

C++ HTTP/WebSocket сервер для выполнения Python кода через GraphQL API.

## ⚠️ Статус проекта

**Версия:** Alpha (v0.1.0-alpha)  
**Стабильность:** Нестабильная  
**Готовность:** 70%

### Известные ограничения:
- [ ] Проблемы с экранированными кавычками в GraphQL
- [ ] Нет обработки таймаутов
- [ ] Ограниченная валидация входных данных

## Возможности

- **GraphQL API** - выполнение Python кода через GraphQL запросы
- **HTTP/WebSocket** - поддержка обоих протоколов
- **Асинхронность** - построен на Boost.Beast и Boost.Asio
- **Безопасность** - изоляция выполнения Python кода

## Быстрый старт

### Сборка
```bash
mkdir build && cd build
cmake ..
make
```

### Запуск
```bash
./python_repl_server --port 8080
```

### Пример запроса
```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { executePython(code: \"print(\\\"Hello, World!\\\")\") { output error executionTime } }"
  }'
```

## API

- `GET /` - проверка состояния сервера
- `POST /graphql` - GraphQL endpoint
- WebSocket - для real-time выполнения

## Требования

- C++17
- Boost.Beast
- nlohmann/json
- Python 3.x

## Структура проекта

```
├── src/           # Исходный код
├── include/       # Заголовочные файлы
├── examples/      # Примеры запросов
└── build/         # Сборка
```