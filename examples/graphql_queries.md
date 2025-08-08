# Примеры GraphQL запросов

## 1. Проверка здоровья сервера
```bash
curl -X GET http://localhost:8080/
```

## 2. Простой вывод
```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { executePython(code: \"print(\\\"Hello, World!\\\")\") { output error executionTime } }"
  }'
```

## 3. Импорт математики и расчет
```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { executePython(code: \"import math\\nresult = math.sqrt(16)\\nprint(f\\\"Квадратный корень из 16: {result}\\\")\") { output error executionTime } }"
  }'
```

## 4. Список с циклом и выводом
```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { executePython(code: \"numbers = [1, 2, 3, 4, 5]\\nfor num in numbers:\\n    print(f\\\"Число: {num}\\\")\\nprint(f\\\"Сумма: {sum(numbers)}\\\")\") { output error executionTime } }"
  }'
```

## 5. Словарь с выводом
```bash
curl -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{
    "query": "query { executePython(code: \"person = {\\\"name\\\": \\\"Иван\\\", \\\"age\\\": 25, \\\"city\\\": \\\"Москва\\\"}\\nfor key, value in person.items():\\n    print(f\\\"{key}: {value}\\\")\") { output error executionTime } }"
  }'
```