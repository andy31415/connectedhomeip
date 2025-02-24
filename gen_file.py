#!/usr/bin/env python3

import textwrap
import click
import jinja2
import random


@click.command()
@click.option(
    "--size",
    default=1024,
    show_default=True,
    type=int,
    help="How many bytes",
)
@click.argument(
    "output",
    type=click.Path(dir_okay=False, file_okay=True, writable=True),
    default="src/app/clusters/operational-credentials-server/size_filler.inc",
)
def main(size, output):
    random.seed(1234)

    data_lines = []
    for i, v in enumerate(random.randbytes(size)):
        if i % 20 == 0:
            data_lines.append([])
        data_lines[-1].append(v % 256)


    with open(output, "wt") as f:
        f.write(
            jinja2.Template(
                textwrap.dedent(
                    """\
                    #include <cstdint>

                    namespace Filler {

                    const uint8_t filler_data[{{cnt}}] = {
                    {%- for line in data_lines %}
                        {% for v in line %} {{'0x%02X' | format(v)}},{% endfor %}
                    {%- endfor %}
                    };

                    } // namespace Filler

                    """
                )
            ).render(cnt=size, data_lines=data_lines)
        )


if __name__ == "__main__":
    main()
