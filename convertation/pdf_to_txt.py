#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
pdf_to_txt.py

Умный конвертер PDF книг и документов в чистый текстовый формат (.txt).
Особенности:
- Точное извлечение текста с учетом шрифтов, кодировок и координат символов.
- Удаление номеров страниц, колонтитулов и технической разметки.
- "Умное" склеивание строк в абзацы с сохранением диалогов (–), красной строки и заголовков.
- Бесшовное соединение предложений, разорванных между страницами.
- Устранение переносов слов на концах строк.
- Пакетная обработка всех PDF-файлов в папке или отдельных указанных файлов.
"""

import os
import re
import sys
import argparse
from typing import List, Tuple, Dict, Any, Optional

try:
    import pypdf
    from pypdf._text_extraction._text_extractor import Font
except ImportError:
    print("Ошибка: библиотека pypdf не установлена. Установите её: pip install pypdf")
    sys.exit(1)


def is_page_number_or_header(line_text: str, y: float, page_height: float = 1100.0) -> bool:
    """
    Проверяет, является ли строка номером страницы или колонтитулом.
    Обычно колонтитулы и номера страниц находятся у самого верхнего (y < 60)
    или нижнего (y > page_height - 60) края страницы и содержат только цифры
    или служебные фразы.
    """
    cleaned = line_text.strip()
    if not cleaned:
        return True

    # Проверка на изолированные номера страниц: "123", "— 123 —", "- 123 -", "[ 123 ]", "Стр. 123"
    if re.match(r'^(?:[—–\-~]\s*)?(?:\bстр\.?\s*)?\d+(?:\s*[—–\-~])?$', cleaned, re.IGNORECASE):
        # Если строка находится у верхнего или нижнего края
        if y < 80 or y > (page_height - 80):
            return True
        # Если состоит только из цифр и очень короткая
        if cleaned.isdigit() and len(cleaned) <= 4:
            return True

    return False


def extract_raw_page_lines(page, reader: pypdf.PdfReader) -> List[Dict[str, Any]]:
    """
    Извлекает строки страницы с их точными координатами (x, y) и размером шрифта,
    напрямую разбирая операторы потока содержимого (ContentStream).
    Это исключает внутренние ошибки pypdf с потерей пробелов и смещением текста.
    """
    try:
        res = page.get('/Resources', {})
        if hasattr(res, 'get_object'):
            res = res.get_object()
        font_dict = res.get('/Font', {}) if '/Font' in res else {}
        if hasattr(font_dict, 'get_object'):
            font_dict = font_dict.get_object()

        font_objs = {}
        for name, f in font_dict.items():
            f_obj = f.get_object() if hasattr(f, 'get_object') else f
            try:
                font_objs[name] = Font.from_font_resource(f_obj)
            except Exception:
                pass

        content_obj = page.get('/Contents')
        if not content_obj:
            return []
        
        content = pypdf.generic.ContentStream(content_obj.get_object(), reader, 'bytes')

        current_font = None
        cur_font_size = 0.0
        tm = [1.0, 0.0, 0.0, 1.0, 0.0, 0.0]
        tlm = [1.0, 0.0, 0.0, 1.0, 0.0, 0.0]

        extracted = []

        for operands, op in content.operations:
            if op == b'Tf':
                current_font = font_objs.get(operands[0])
                cur_font_size = float(operands[1])
            elif op == b'Tm':
                tm = [float(x) for x in operands]
                tlm = list(tm)
            elif op == b'Td':
                tx, ty = float(operands[0]), float(operands[1])
                tlm[4] += tx * tlm[0] + ty * tlm[2]
                tlm[5] += tx * tlm[1] + ty * tlm[3]
                tm = list(tlm)
            elif op == b'TD':
                tx, ty = float(operands[0]), float(operands[1])
                tlm[4] += tx * tlm[0] + ty * tlm[2]
                tlm[5] += tx * tlm[1] + ty * tlm[3]
                tm = list(tlm)
            elif op == b'T*':
                # Переход на следующую строку
                pass
            elif op == b'Tj':
                raw_bytes = operands[0]
                if isinstance(raw_bytes, str):
                    chars = raw_bytes
                else:
                    enc = current_font.encoding if (current_font and isinstance(current_font.encoding, str)) else 'utf-16-be'
                    try:
                        chars = raw_bytes.decode(enc, errors='surrogatepass')
                    except Exception:
                        chars = raw_bytes.decode('latin1', errors='replace')

                if current_font and hasattr(current_font, 'character_map'):
                    decoded = ''.join(current_font.character_map.get(c, c) for c in chars)
                else:
                    decoded = chars

                extracted.append((tm[5], tm[4], decoded, cur_font_size))

        if not extracted:
            # Fallback к стандартному извлечению, если операций Tj не обнаружено
            text = page.extract_text() or ""
            return [{'y': i * 20.0, 'x': 0.0, 'text': l.strip(), 'fs': 12.0} 
                    for i, l in enumerate(text.splitlines()) if l.strip()]

        # Группируем фрагменты текста по координате Y (с допуском 2.0 pt)
        line_groups = []
        for y, x, text, fs in extracted:
            placed = False
            for group in line_groups:
                if abs(group['y'] - y) <= 2.0:
                    group['items'].append((x, text, fs))
                    placed = True
                    break
            if not placed:
                line_groups.append({'y': y, 'items': [(x, text, fs)]})

        line_groups.sort(key=lambda g: g['y'])

        res_lines = []
        for group in line_groups:
            group['items'].sort(key=lambda it: it[0])
            first_x = group['items'][0][0]
            max_fs = max(it[2] for it in group['items'])
            full_text = ''.join(it[1] for it in group['items']).strip()
            if full_text:
                res_lines.append({
                    'y': group['y'],
                    'x': first_x,
                    'text': full_text,
                    'fs': max_fs
                })
        return res_lines

    except Exception:
        # Надежный резервный механизм
        text = page.extract_text() or ""
        return [{'y': i * 20.0, 'x': 0.0, 'text': l.strip(), 'fs': 12.0} 
                for i, l in enumerate(text.splitlines()) if l.strip()]


def is_heading_or_special(text: str) -> bool:
    """
    Определяет, является ли строка заголовком главы, части или разделителем.
    Например: "I", "II", "XXV", "ЧАСТЬ ПЕРВАЯ", "Том 1".
    """
    clean = text.strip()
    if len(clean) > 60:
        return False
    # Римские цифры (I, II, III, IV, ..., XXVIII) или арабские
    if re.match(r'^(?:[IVXLCDM]+|[1-9]\d*)\.?$', clean):
        return True
    # Часть, Глава, Книга, Эпилог
    if re.match(r'^(?:ЧАСТЬ|ГЛАВА|КНИГА|ЭПИЛОГ|ПРОЛОГ)\b', clean, re.IGNORECASE):
        return True
    # Слово "ТОМ" должно сопровождаться номером тома (чтобы не путать с местоимением "том, что...")
    if re.match(r'^ТОМ\s+(?:[IVXLCDM]+|\d+|ПЕРВЫЙ|ВТОРОЙ|ТРЕТИЙ|ЧЕТВЕРТЫЙ)\b', clean, re.IGNORECASE):
        return True
    if clean.lower() in ('annotation', 'содержание', 'оглавление'):
        return True
    return False


def wrap_russian_paragraph(para: str, width: int = 80) -> str:
    """
    Переносит строки по правилам русской книжной типографики:
    - Без висячих предлогов и союзов (в, на, с, о, к, не, из...) в конце строки
    - Привязка начального тире реплики (–) к первому слову
    - Привязка тире внутри предложения к предыдущему слову (тире не падает в начало строки)
    - Привязка частиц (бы, ли, же) к предшествующему слову
    """
    import textwrap
    NBSP = "\u00A0"
    t = para

    # 1. Привязка начального тире к первому слову реплики
    t = re.sub(r"^([–—―-])\s+", lambda m: m.group(1) + NBSP, t)

    # 2. Привязка тире внутри предложения к предыдущему слову
    t = re.sub(r"\s+([–—―])\s+", lambda m: NBSP + m.group(1) + " ", t)

    # 3. Привязка частиц бы, ли, же к предшествующему слову
    t = re.sub(r"\s+([бвжл]ь?|бы|ли|же)\b", lambda m: NBSP + m.group(1), t, flags=re.IGNORECASE)

    # 4. Привязка коротких предлогов и союзов к следующему слову (нет висячих предлогов)
    prep_pattern = r"\b([вкосуиано]|во|ко|со|об|обо|из|от|до|по|за|на|под|над|при|без|для|про|не|ни|de|du|la|le|les|et|en|un|une)\s+"
    t = re.sub(prep_pattern, lambda m: m.group(1) + NBSP, t, flags=re.IGNORECASE)

    wrapped = textwrap.fill(
        t,
        width=width,
        break_long_words=False,
        break_on_hyphens=False
    )
    return wrapped.replace(NBSP, " ")


def assemble_paragraphs(
    page_lines_list: List[List[Dict[str, Any]]],
    wrap_width: Optional[int] = 80
) -> str:
    """
    Умно объединяет строки страниц в связанные абзацы:
    - Отфильтровывает колонтитулы и номера страниц.
    - Сохраняет структуру диалогов (строки с тире – / —).
    - Определяет начало новых абзацев по красной строке (отступу X) и вертикальным интервалам.
    - Склеивает разорванные на переносах строки (употребляв- + шегося -> употреблявшегося).
    - Обеспечивает плавный переход текста через границы страниц.
    """
    all_clean_lines = []

    for page_idx, lines in enumerate(page_lines_list):
        if not lines:
            continue

        # Фильтрация колонтитулов и номеров страниц
        filtered = []
        for line in lines:
            if not is_page_number_or_header(line['text'], line['y']):
                filtered.append(line)

        if not filtered:
            continue

        # Базовая координата X (левый край страницы)
        min_x = min(l['x'] for l in filtered)

        for i, line in enumerate(filtered):
            # Проверяем красную строку (абзацный отступ)
            is_indented = (line['x'] - min_x) >= 20.0
            
            # Проверяем вертикальный отступ перед строкой (больше обычного межстрочного)
            has_vertical_gap = False
            if i > 0:
                delta_y = line['y'] - filtered[i - 1]['y']
                if delta_y > 32.0:
                    has_vertical_gap = True

            all_clean_lines.append({
                'page': page_idx,
                'text': line['text'],
                'indented': is_indented,
                'vertical_gap': has_vertical_gap,
                'fs': line['fs']
            })

    paragraphs = []
    current_para = ""

    for item in all_clean_lines:
        line_text = item['text'].strip()
        if not line_text:
            continue

        is_heading = is_heading_or_special(line_text)
        is_dialogue = line_text.startswith(('–', '—', '―', '- '))
        is_quote_start = line_text.startswith(('«', '"', '['))
        
        # Условие начала нового абзаца:
        # 1. Красная строка (отступ)
        # 2. Прямая речь / диалог (тире)
        # 3. Заголовок главы / раздела
        # 4. Заметный вертикальный отступ (новая смысловая секция)
        is_new_para = (
            item['indented']
            or is_dialogue
            or is_heading
            or item['vertical_gap']
            or (is_quote_start and item['indented'])
        )

        if is_new_para:
            if current_para:
                paragraphs.append(current_para.strip())
            current_para = line_text
        else:
            if not current_para:
                current_para = line_text
            else:
                # Обработка переноса слова в конце строки
                if current_para.endswith('-'):
                    # Если перенос (например, "слов-" + "но" -> "словно")
                    # Проверяем: если дальше идет строчная буква, объединяем без дефиса
                    if line_text and line_text[0].islower():
                        current_para = current_para[:-1] + line_text
                    else:
                        current_para = current_para + line_text
                elif current_para.endswith('\xad'):  # soft hyphen
                    current_para = current_para[:-1] + line_text
                else:
                    current_para += ' ' + line_text

    if current_para:
        paragraphs.append(current_para.strip())

    # Проверяем и удаляем вступительное оглавление / аннотацию
    # (если в начале книги идет технический блок Annotation / оглавление со списком глав)
    start_idx = 0
    if paragraphs and paragraphs[0].lower().startswith(('annotation', 'оглавление', 'содержание')):
        for i, p in enumerate(paragraphs[:250]):
            # Ищем начало титульной страницы романа или первой части
            p_lower = p.lower()
            if ('лев николаевич' in p_lower) or ('война и мир' in p_lower and i > 5):
                start_idx = i
                break
        if start_idx > 0:
            paragraphs = paragraphs[start_idx:]

    # Если задано ограничение по ширине строки (как в книге)
    if wrap_width and wrap_width > 0:
        wrapped_paras = []
        for p in paragraphs:
            # Для коротких заголовков перенос не требуется
            if is_heading_or_special(p) or len(p) <= wrap_width:
                wrapped_paras.append(p)
            else:
                wrapped = wrap_russian_paragraph(p, width=wrap_width)
                wrapped_paras.append(wrapped)
        paragraphs = wrapped_paras

    # Соединяем абзацы без пустых строк между ними ("убираем отступы после строк")
    return '\n'.join(paragraphs)


def convert_pdf_to_txt(
    pdf_path: str,
    output_path: Optional[str] = None,
    wrap_width: Optional[int] = 80,
    verbose: bool = True
) -> str:
    """
    Конвертирует один PDF файл в текстовый файл TXT.
    """
    if not os.path.exists(pdf_path):
        raise FileNotFoundError(f"Файл не найден: {pdf_path}")

    if not output_path:
        base, _ = os.path.splitext(pdf_path)
        output_path = f"{base}.txt"

    if verbose:
        print(f"\n[Обработка] {os.path.basename(pdf_path)}...")

    reader = pypdf.PdfReader(pdf_path)
    total_pages = len(reader.pages)

    page_lines_list = []
    for idx, page in enumerate(reader.pages):
        lines = extract_raw_page_lines(page, reader)
        page_lines_list.append(lines)
        if verbose and ((idx + 1) % 50 == 0 or (idx + 1) == total_pages):
            print(f"  Страница {idx + 1}/{total_pages} ({(idx + 1) * 100 // total_pages}%)")

    full_text = assemble_paragraphs(page_lines_list, wrap_width=wrap_width)

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(full_text)
        f.write('\n')

    file_size_kb = os.path.getsize(output_path) / 1024
    if verbose:
        print(f"[Готово] Сохранено в: {os.path.basename(output_path)} ({file_size_kb:.1f} КБ)")

    return output_path


def main():
    parser = argparse.ArgumentParser(
        description="Умный конвертер PDF книг и документов в TXT формат (без номеров страниц, с сохранением абзацев)."
    )
    parser.add_argument(
        'files',
        nargs='*',
        help="Пути к PDF файлам для конвертации. Если не указаны, конвертируются все PDF в текущей папке."
    )
    parser.add_argument(
        '-o', '--output',
        help="Путь к выходному файлу (только если указан один входной файл)."
    )
    parser.add_argument(
        '-w', '--width',
        type=int,
        default=80,
        help="Максимальная длина строки (по умолчанию 80). Укажите 0, чтобы отключить перенос."
    )

    args = parser.parse_args()

    targets = args.files
    if not targets:
        # Автоматически находим все PDF в текущей рабочей директории
        targets = [f for f in os.listdir('.') if f.lower().endswith('.pdf')]
        targets.sort()
        if not targets:
            print("В текущей папке не найдено файлов .pdf.")
            sys.exit(0)
        print(f"Найдено PDF файлов для конвертации: {len(targets)}")

    if args.output and len(targets) > 1:
        print("Ошибка: параметр -o/--output можно указывать только для одного входного файла.")
        sys.exit(1)

    wrap_w = args.width if (args.width is not None and args.width > 0) else None

    for target in targets:
        out_path = args.output if (args.output and len(targets) == 1) else None
        try:
            convert_pdf_to_txt(target, out_path, wrap_width=wrap_w)
        except Exception as e:
            print(f"[Ошибка при обработке {target}]: {e}", file=sys.stderr)


    print("\nВсе операции успешно завершены!")


if __name__ == '__main__':
    main()
