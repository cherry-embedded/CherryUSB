#!/usr/bin/env python3
#
# Copyright (c) 2026, CherryUSB contributors
#
# SPDX-License-Identifier: Apache-2.0
#
"""Check and fix missing spaces between CJK chars and adjacent Latin/digit words in docs.

中文排版规范：中文字符与英文字母/数字（ASCII 词）之间应有一个空格。

* 只做"插入空格"这一种改动，绝不增删或改写其它字符。
* ASCII 词内部永不拆分："USB2.0"、"0x01"、"2.5us"、"D+"、"D+/D-" 保持连写，
  仅在整词与中文字符的交界处补空格。
* 已存在的空格不动，不会产生连续空格。
* 全角标点（，。、（）％等）不属于"中文字符"，不参与判断，因此与其相邻的
  ASCII 内容不会被动到。

RST 感知：
* 跳过字面块（``::`` 引入的缩进块）与 .. code-block:: / .. code:: 整段；
* .. figure:: / .. image:: 等只带参数的指令行不处理；
* .. note::正文 这类"指令与正文同行"的行只处理 ``::`` 之后的正文片段；
* 行内代码/链接（``...`` 与 `...`）的内容不处理。

用法：
    python tools/fix_zh_spacing.py <file_or_dir> [...]      # 只检测，发现问题退出码为 1
    python tools/fix_zh_spacing.py --fix <file_or_dir> [...]  # 直接补空格写回

目录参数会递归扫描其中的 .rst / .md 文件。
"""

import argparse
import os
import re
import sys

# 视为"中文字符"的码位区间（CJK 统一表意文字及其扩展）
_CJK_RANGES = (
    (0x3400, 0x4DBF),   # CJK Extension A
    (0x4E00, 0x9FFF),   # CJK Unified Ideographs
    (0xF900, 0xFAFF),   # CJK Compatibility Ideographs
)

# 允许作为 ASCII 词内部或边缘一部分的符号（词内必须含字母/数字才会补空格）。
# 注意刻意排除 * _ = , ; : ( ) 等 RST 标记或中文句内标点，避免拆坏 **加粗** / 链接 / 等号算式。
_WORD_SYMBOLS = set("+-./#%~")

# 只带参数、没有正文的 RST 指令：整行不处理
_ARG_ONLY_DIRECTIVES = {
    "figure", "image", "include", "literalinclude", "code", "code-block",
    "csv-table", "toctree", "math", "raw", "highlight", "default-role",
    "contents", "cssclass", "meta", "sectionauthor", "moduleauthor",
}

# 其后整段缩进块为代码/字面内容，需整段跳过
_CODE_BLOCK_DIRECTIVES = {"code", "code-block", "literalinclude"}

_DIRECTIVE_RE = re.compile(r"^\.\.\s+([A-Za-z][\w-]*)::")


def is_cjk(ch):
    """是否是中文字符（表意文字）。全角标点不属于中文字符。"""
    if ch == "\u3007":  # 〇
        return True
    code = ord(ch)
    return any(lo <= code <= hi for lo, hi in _CJK_RANGES)


def is_ascii_alnum(ch):
    return "a" <= ch <= "z" or "A" <= ch <= "Z" or "0" <= ch <= "9"


def is_word_ascii(ch):
    return is_ascii_alnum(ch) or ch in _WORD_SYMBOLS


def _split_eol(raw):
    """拆出行内容与行尾，保留原行尾风格（LF / CRLF）。"""
    if raw.endswith("\r\n"):
        return raw[:-2], "\r\n"
    if raw.endswith("\n") or raw.endswith("\r"):
        return raw[:-1], raw[-1:]
    return raw, ""


def _protected_ranges(seg):
    """行内 ``...`` / `...` 内容的保护区（左闭右开），供跳过使用。"""
    runs = []
    i, n = 0, len(seg)
    while i < n:
        if seg[i] == "`":
            j = i
            while j < n and seg[j] == "`":
                j += 1
            runs.append((i, j))
            i = j
        else:
            i += 1
    ranges = []
    for k in range(0, len(runs) - 1, 2):
        ranges.append((runs[k][1], runs[k + 1][0]))
    return ranges


def fix_segment(seg):
    """对一段正文补空格，返回 (修复后文本, 补空格处数)。"""
    if not seg:
        return seg, 0
    protected = _protected_ranges(seg)
    in_span = [False] * len(seg)
    for start, end in protected:
        for k in range(start, end):
            in_span[k] = True

    inserts = []
    n = len(seg)
    for i in range(n - 1):
        if in_span[i] or in_span[i + 1]:
            continue
        a, b = seg[i], seg[i + 1]
        if is_cjk(a) and is_word_ascii(b):
            j = i + 1
            while j < n and is_word_ascii(seg[j]):
                j += 1
            # 词内必须含字母/数字才算 ASCII 词；单独的符号（如 "全速/高速" 中的 "/"）不补
            if any(is_ascii_alnum(c) for c in seg[i + 1:j]):
                inserts.append(i + 1)
        elif is_word_ascii(a) and is_cjk(b):
            j = i
            while j >= 0 and is_word_ascii(seg[j]):
                j -= 1
            if any(is_ascii_alnum(c) for c in seg[j + 1:i + 1]):
                inserts.append(i + 1)

    if not inserts:
        return seg, 0
    parts = []
    prev = 0
    for pos in inserts:
        parts.append(seg[prev:pos])
        parts.append(" ")
        prev = pos
    parts.append(seg[prev:])
    return "".join(parts), len(inserts)


def _process_file(path, fix):
    """处理单个文件，返回 (变更列表, 新文本或 None)。"""
    try:
        with open(path, "r", encoding="utf-8", newline="") as handle:
            text = handle.read()
    except (OSError, UnicodeDecodeError) as exc:
        print(f"skip {path}: {exc}", file=sys.stderr)
        return [], None

    lines = text.splitlines(True)
    result = []
    changes = []
    block = None  # None / "code" / "literal"
    for lineno, raw in enumerate(lines, 1):
        content, eol = _split_eol(raw)
        if block in ("code", "literal"):
            if content.strip() == "" or content[:1] in (" ", "\t"):
                result.append(raw)
                continue
            block = None

        indent = content[: len(content) - len(content.lstrip())]
        rest = content[len(indent):]
        if rest.startswith(".. "):
            match = _DIRECTIVE_RE.match(rest)
            if not match:
                # 无 "::" 的 RST 注释行，原样保留
                result.append(raw)
                continue
            keyword = match.group(1)
            if keyword in _ARG_ONLY_DIRECTIVES:
                if keyword in _CODE_BLOCK_DIRECTIVES:
                    block = "code"
                result.append(raw)
                continue
            body = rest[match.end():]
            if body:
                fixed, count = fix_segment(body)
                fixed_line = indent + rest[:match.end()] + fixed + eol
                if count:
                    changes.append((lineno, content, indent + rest[:match.end()] + fixed, count))
                result.append(fixed_line)
                continue
            result.append(raw)
            continue

        fixed, count = fix_segment(content)
        if count:
            changes.append((lineno, content, fixed, count))
            result.append(fixed + eol)
        else:
            result.append(raw)

        if content.rstrip().endswith("::"):
            block = "literal"

    new_text = "".join(result)
    return changes, new_text


def _iter_files(paths):
    for path in paths:
        if os.path.isdir(path):
            for root, _dirs, files in os.walk(path):
                for name in sorted(files):
                    if name.endswith((".rst", ".md")):
                        yield os.path.join(root, name)
        else:
            yield path


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="检查并补全中文字符与英文字母/数字之间的空格（RST/Markdown 文档）"
    )
    parser.add_argument("paths", nargs="+", metavar="PATH", help="文件或目录（目录会递归扫描 .rst/.md）")
    parser.add_argument("--fix", action="store_true", help="自动补空格并写回文件；默认仅检测")
    args = parser.parse_args(argv)

    total_issues = 0
    total_files_changed = 0
    files_processed = 0
    for path in _iter_files(args.paths):
        changes, new_text = _process_file(path, args.fix)
        if new_text is None:
            continue
        files_processed += 1
        if not changes:
            continue
        total_files_changed += 1
        file_issues = sum(count for _ln, _old, _new, count in changes)
        total_issues += file_issues

        if args.fix:
            try:
                with open(path, "w", encoding="utf-8", newline="") as handle:
                    handle.write(new_text)
            except OSError as exc:
                print(f"error write {path}: {exc}", file=sys.stderr)
                return 2
            print(f"{path}: fixed {file_issues} missing spaces")
        else:
            print(f"{path}: {file_issues} missing spaces in {len(changes)} line(s)")
            for lineno, old, new, count in changes:
                print(f"  {path}:{lineno}: {count} missing space(s)")
                print(f"    - {old}")
                print(f"    + {new}")

    if args.fix:
        print(f"done: {total_files_changed}/{files_processed} file(s) changed, "
              f"{total_issues} space(s) inserted")
        return 0
    if total_issues:
        print(f"found {total_issues} missing space(s) in {total_files_changed}/{files_processed} file(s)")
        return 1
    print(f"ok: no missing spaces in {files_processed} file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
