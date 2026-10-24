import os
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, jsonify, render_template, request


app = Flask(__name__)
DATABASE = Path(os.environ.get("PRESENCE_DATABASE", Path(__file__).with_name("presencas.db")))


def get_db_connection():
    connection = sqlite3.connect(DATABASE)
    connection.row_factory = sqlite3.Row
    return connection


def initialize_database():
    DATABASE.parent.mkdir(parents=True, exist_ok=True)
    with get_db_connection() as connection:
        connection.execute(
            """
            CREATE TABLE IF NOT EXISTS presencas (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                sensor_id TEXT NOT NULL DEFAULT 'sensor-1',
                registrado_em TEXT NOT NULL
            )
            """
        )


@app.route("/")
def index():
    with get_db_connection() as connection:
        rows = connection.execute(
            "SELECT id, sensor_id, registrado_em FROM presencas ORDER BY id DESC LIMIT 200"
        ).fetchall()
        total = connection.execute("SELECT COUNT(*) FROM presencas").fetchone()[0]

    presencas = []
    for row in rows:
        data_hora = datetime.fromisoformat(row["registrado_em"])
        presencas.append(
            {
                "id": row["id"],
                "sensor_id": row["sensor_id"],
                "registrado_em": data_hora.astimezone().strftime("%d/%m/%Y %H:%M:%S"),
            }
        )
    return render_template("index.html", presencas=presencas, total=total)


@app.route("/presenca", methods=["GET"])
@app.route("/api/presenca", methods=["GET"])
def registrar_presenca():
    """Registra a hora em que o servidor recebeu o acionamento do sensor."""
    sensor_id = request.args.get("sensor_id", "sensor-1").strip() or "sensor-1"
    print(request.headers)
    if len(sensor_id) > 100:
        return jsonify(erro="sensor_id deve ter no máximo 100 caracteres"), 400

    registrado_em = datetime.now(timezone.utc).isoformat(timespec="seconds")
    with get_db_connection() as connection:
        cursor = connection.execute(
            "INSERT INTO presencas (sensor_id, registrado_em) VALUES (?, ?)",
            (sensor_id, registrado_em),
        )
        presence_id = cursor.lastrowid

    return jsonify(
        status="registrada",
        id=presence_id,
        sensor_id=sensor_id,
        registrado_em=registrado_em,
    ), 201


initialize_database()


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", "5000")), debug=False)
