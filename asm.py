import argparse
from collections import defaultdict
import os


def warn(msg: str):
    print(f"WARNING: {msg}")


def parse_label(s: str) -> str:
    if not s.startswith("."):
        raise Exception("label must start with .")
    s = s[1:]
    if not s.isalpha():
        raise Exception("label must consist only of letters")
    return s


def parse_var(s: str) -> str:
    if s.startswith("var"):
        i = int(s[3:])
        if 0 < i > 255:
            raise Exception(f"var index {i} out of range")
        return f"{i:08b}"
    else:
        raise Exception("var expected")


def parse_bit_source(s: str) -> str:
    res = ""

    if s.startswith("mem"):
        c = s.find("]")
        a, b = s[4:c], s[c + 2 : -1]
        i1 = parse_var(a)
        res += "1"
        try:
            i2 = parse_var(b)
            res += "1"
        except:
            i2 = f"{int(b, 0):064b}"
            res += "0"

    elif s.startswith("var"):
        c = s.find("[")
        a, b = s[:c], s[c + 1 : -1]
        i1 = parse_var(a)
        res += "0"
        try:
            i2 = parse_var(b)
            res += "1"
        except:
            i2 = f"{int(b, 0):06b}"
            res += "0"

    else:
        raise Exception("bit source must be from var or mem")

    res += i1 + i2
    return res


class JmpDest:
    jmpdst: int = -1
    uses: set[int]

    def __init__(self):
        self.uses = set()

    def __str__(self):
        return f"jmpdest {self.jmpdst} uses {self.uses}"


def process(filename: str, offset=0):
    labels: dict[str, JmpDest] = defaultdict(JmpDest)
    res = ""

    def plabel(s: str):
        nonlocal res
        nonlocal labels
        label = parse_label(s)
        labels[label].uses.add(len(res))
        res += "-" * 64

    with open(filename, "r") as f:
        file_dir = os.path.dirname(filename)
        file_dir = file_dir + "/" if len(file_dir) > 0 else file_dir
        try:
            line_n = 0
            try:
                for line in f:
                    line_n += 1
                    line = line.strip()
                    line = line.split("//", maxsplit=1)[0]

                    if line in ("", "}", "{"):
                        continue

                    elif line.startswith(">"):
                        res += "".join(
                            f"{ord(ch):016b}"
                            for ch in line[1:].encode().decode("unicode_escape")
                        )
                        continue

                    elif line.startswith("#import"):
                        res += process(
                            file_dir + line.split()[1].strip(), offset=offset + len(res)
                        )
                        continue

                    elif line.startswith("."):
                        label = line[1:]
                        if not label.isalpha():
                            raise Exception("label must consist only of letters")
                        if labels[label].jmpdst != -1:
                            raise Exception("label was already defined")
                        labels[label].jmpdst = offset + len(res)
                        continue

                    elif line.startswith("jmp"):
                        _, label = line.split()
                        res += "00"
                        plabel(label)
                        continue

                    elif line.startswith("return"):
                        _, var = line.split()
                        res += "010" + parse_var(var)
                        continue

                    elif line == "pull":
                        res += "10010"
                        continue

                    elif line.startswith("new var"):
                        if "=" not in line:
                            res += "100111"
                            continue
                        else:
                            _, src = (e.strip() for e in line.split("="))

                            res += "100110"
                            try:
                                res += "0" + parse_var(src)
                                continue
                            except:
                                pass

                            res += "1"
                            try:
                                res += f"{int(src, 0):064b}"
                                continue
                            except:
                                pass

                            try:
                                plabel(src)
                                continue
                            except:
                                pass

                            raise Exception(
                                "invalid second parameter, must be a label, a constant, or a var"
                            )

                    elif line.startswith("dealloc"):
                        res += "10110" + parse_var(line[8:-1])
                        continue

                    elif "sizeof(mem[" in line and "." not in line:
                        dst, src = (e.strip() for e in line.split("="))
                        res += "1010" + parse_var(dst) + parse_var(src[11:-2])
                        continue

                    elif "alloc" in line and "." not in line:
                        dst, src = (e.strip() for e in line.split("="))

                        dst = parse_var(dst)
                        src = src[6:-1]

                        res += "10111"
                        try:
                            res += "1" + dst + parse_var(src)
                            continue
                        except:
                            pass

                        try:
                            res += f"0{dst}{int(src, 0):064b}"
                            continue
                        except:
                            pass

                        raise Exception(
                            "invalid second parameter, must be a constant, or a var"
                        )

                    elif "(" in line:
                        dst, src = (e.strip() for e in line.split("="))

                        dst = parse_var(dst)

                        if "func[" in src:
                            s = src.find("]")
                            res += (
                                "0110"
                                + dst
                                + parse_var(src[5:s])
                                + parse_var(src[s + 2 : -1])
                            )

                        else:
                            s = src.find("(")
                            res += "0111" + dst

                            plabel(src[:s])

                            res += f"{int(src[s + 1 : -1], 0):08b}"

                        continue

                    elif line.startswith("if !"):
                        c = line.find(",")
                        res += "111" + parse_bit_source(line[4:c])
                        plabel(line[c + 6 :])
                        continue

                    elif "[" in line:
                        dst, src = (e.strip() for e in line.split("="))
                        dst = parse_bit_source(dst)
                        res += "110"
                        try:
                            res += "1" + dst + parse_bit_source(src)
                        except:
                            res += "0" + dst + f"{int(src, 0):01b}"
                        continue

                    elif "var" in line:
                        dst, src = (e.strip() for e in line.split("="))

                        dst = parse_var(dst)

                        res += "1000"
                        try:
                            res += "0" + dst + parse_var(src)
                            continue
                        except:
                            pass

                        res += "1" + dst
                        try:
                            res += f"{int(src, 0):064b}"
                            continue
                        except:
                            pass

                        try:
                            plabel(src)
                            continue
                        except:
                            pass

                        raise Exception(
                            "invalid second parameter, must be a label, a constant, or a var"
                        )

                    raise Exception(f"<{line}> no such command")

            except Exception as e:
                raise Exception(f"Error on line {line_n}") from e

            for label in labels:
                jmpdst = labels[label].jmpdst
                uses = labels[label].uses
                if len(uses) == 0:
                    warn(f"label <{label}> is not used")
                if jmpdst == -1:
                    raise Exception(f"label <{label}> is not defined")
                for pos in uses:
                    res = res[:pos] + f"{jmpdst:064b}" + res[pos + 64 :]

            return res

        except Exception as e:
            raise Exception(f"in file <{filename}>") from e


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="asm compiler",
        description="dirty tenandayo asm compiler",
    )

    parser.add_argument(
        "input",
        type=str,
        help="code file",
    )

    parser.add_argument(
        "output",
        type=str,
        help="output file",
    )

    args = parser.parse_args()

    res = process(args.input)

    with open(args.output, "w") as f:
        f.write(res)
