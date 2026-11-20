"""Vercel entrypoint: serves the FastAPI app from server/main.py.

Vercel looks for a FastAPI instance named `app` in app.py at the project root.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "server"))

from main import app  # noqa: E402,F401
