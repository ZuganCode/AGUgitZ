# Проект - консольное приложение архиватор.

# Команда 7: Проект Advanced C

# Участники команды:

- Бибиков Никита, группа 4.307-1
- Кравченко Михаил, группа 4.307-1
- Шмаков Григорий, группа 4.307-1
- Ляпин Ермак, группа 4.307-1
- Буренкин Егор, группа 4.307-1
- Пашин Унан, группа 4.307-1

# Описание

Проект по разработке консольного приложения архиватора, 
предназначенного для архивации (сжатия) и распаковки файлов.

# Функциональные возможности

## Архивация файла

- Возможность выбора файла для архивации.
- Возможность задать имя выходного архивного файла.

## Распаковка архива

- Возможность выбора файла для разархивации.
- Возможность выбора кол-ва разархивируемых файлов

## Прочие функции

- Добавление файла в существующий архив.
- Удаление файлов из архива.

### Обработка ошибок

- Обработана возможность отсутствия файла в указанной директории.
- Предотвращен возможный ввод несуществующей команды.

## Параметры командной строки

- `--create` или `-с`: Команда для архивации файлов, используется по шаблону [--create имя_создаваемого_файла --file имя_архивируемого_файла].
- `--extract` или `-x`: Команда для извлечения файлов из архива, используется по шаблону [--extract имя_разархивируемого_файла кол-во_разархивируемых_файлов порядковые_номера_файлов(через пробел, в кол-ве, указанном в предыдущей переменной).
- `--list` или `-l`: Команда для просмотра содержимого архива, используется по шаблону [--list путь_к_архиву].
- `--add` или `-a`: Команда для добавления в архив нового файла, используется по шаблону[--add имя_архива имя_добавляемого_файла].
- `--delete` или `-d`: Команда для удаления из архива файла, используется по шаблону[--delete имя_разархивируемого_файла кол-во_разархивируемых_файлов порядковые_номера_файлов(через пробел, в кол-ве, указанном в предыдущей переменной)].

## Установка

### Зависимости кода

- Компилятор для языка Си gcc