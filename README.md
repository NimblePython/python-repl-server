# Python REPL Server

C++ HTTP/WebSocket сервер для выполнения Python кода через GraphQL-подобный API.

## ⚠️ Статус проекта

**Версия:** Alpha (v0.1.0-alpha)  
**Стабильность:** Нестабильная  
**Готовность:** 70%

### Известные ограничения:
- [ ] Проблемы с экранированными кавычками в GraphQL
- [ ] Нет обработки таймаутов
- [ ] Ограниченная валидация входных данных

## Возможности

- **GraphQL-подобный API** - выполнение Python кода через JSON запросы в GraphQL формате
- **HTTP/WebSocket** - поддержка обоих протоколов
- **Асинхронность** - построен на Boost.Beast и Boost.Asio
- **Безопасность** - изоляция выполнения Python кода

## ⚠️ Ограничения API

Это **НЕ полноценный GraphQL** сервер. Реализовано:
- JSON формат запросов
- Endpoint `/graphql`
- Операция `executePython(code: String)`
- Базовая интроспекция (`__schema`, `__type`)
- Простая валидация запросов
- GraphQL схема с типами `Query` и `PythonResult`

**НЕ поддерживается:**
- Полная валидация запросов
- Переменные и фрагменты
- Подписки и мутации
- Директивы
- Сложные типы данных

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
- `POST /graphql` - GraphQL-подобный endpoint
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