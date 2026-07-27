import io
import unittest

from tools.analyze_refresh_metrics import parse_metric_line, percentile, summarize, write_markdown


class RefreshMetricsParserTest(unittest.TestCase):
    def test_parser_ignores_noise_and_accepts_prefixed_serial_text(self):
        self.assertIsNone(parse_metric_line("ordinary diagnostic"))
        metric = parse_metric_line(
            "[123] M5EPUB_METRIC,type=page_turn,kind=prefetched_next,"
            "mode=fast,region=body,total_us=100")
        self.assertEqual("prefetched_next", metric["kind"])
        self.assertEqual("100", metric["total_us"])

    def test_summary_groups_and_calculates_percentiles(self):
        lines = [
            "M5EPUB_METRIC,type=page_turn,kind=cached_next,mode=fast,region=full,total_us=100\n",
            "M5EPUB_METRIC,type=page_turn,kind=cached_next,mode=fast,region=full,total_us=200\n",
            "M5EPUB_METRIC,type=page_turn,kind=cached_next,mode=text,region=full,total_us=500\n",
        ]
        rows = [row for row in summarize(lines) if row["metric"] == "total_us"]
        self.assertEqual(2, len(rows))
        fast = next(row for row in rows if row["mode"] == "fast")
        self.assertEqual(2, fast["count"])
        self.assertEqual(150, fast["median"])
        self.assertEqual(195, fast["p95"])

    def test_single_sample_percentile_and_markdown(self):
        self.assertEqual(42, percentile([42], 0.95))
        rows = summarize([
            "M5EPUB_METRIC,type=page_turn,kind=menu_return,mode=quality,region=full,total_us=42"
        ])
        output = io.StringIO()
        write_markdown(rows, output)
        self.assertIn("menu_return", output.getvalue())
        self.assertIn("total_us", output.getvalue())


if __name__ == "__main__":
    unittest.main()
