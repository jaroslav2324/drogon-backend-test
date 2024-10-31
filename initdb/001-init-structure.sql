BEGIN;

CREATE TABLE cities(
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL
);

CREATE TYPE CLOUDINESS_TYPE AS ENUM ('Облачно', 'Переменная облачность', 'Солнечно');

CREATE TABLE forecasts(
    id SERIAL PRIMARY KEY,
    date DATE NOT NULL,
    temperature NUMERIC NOT NULL,
    cloudiness CLOUDINESS_TYPE NOT NULL,
    city_id INT REFERENCES cities(id) ON DELETE CASCADE
);

COMMIT;