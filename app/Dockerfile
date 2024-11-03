FROM drogonframework/drogon:latest

WORKDIR /app
RUN /bin/sh -c "drogon_ctl create project weather_backend"

WORKDIR /app/weather_backend
RUN rm -f main.cc
RUN rm -f config.json
COPY ./src/main.cc ./main.cc
COPY ./config.json ./config.json

WORKDIR /app/weather_backend/build

RUN cmake .. && make

ENTRYPOINT [ "./weather_backend" ]