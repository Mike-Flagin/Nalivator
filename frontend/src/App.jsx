import {useState} from 'react';

import './App.css'
import Spinner from "./components/Spinner/Spinner.jsx";
import Accordion from "./components/Accordion/Accordion.jsx";

function App() {
    const [portion, setPortion] = useState(5);

    const clickHandler = () => {
        console.log(portion);
    }


    const elementList = [
        {
            "id": 1,
            "slot": "Водичка"
        },
        {
            "id": 2,
            "slot": "Огненная водичка\uD83D\uDD25"
        },
        {
            "id": 3,
            "slot": "Чай улун"
        }
    ]
    return (
        <>
            <section id="center">
                <Spinner
                    val={portion}
                    min={1}
                    max={10}
                    step={0.5}
                    onChange={val => setPortion(val)}
                />
            </section>


                <Accordion
                    title = "Game"
                    condition = {false}
                    slot_component = <Spinner
                        val={portion}
                        min={1}
                        max={10}
                        step={0.5}
                        onChange={val => setPortion(val)}
                    />

                />


        </>
    )
}

export default App
